#pragma once

#include "casioxw/ParamModel.h"
#include "casioxw/SysExCodec.h"

#include <juce_audio_devices/juce_audio_devices.h>

#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace casioxw
{
    /** A full SysEx byte frame, F0 ... F7 inclusive — the same representation
        SysExCodec::encode()/decode() use. */
    using SysExFrame = std::vector<std::uint8_t>;

    /** Device enumeration for the "let the user pick a MIDI port" UI. Thin wrapper over
        juce::MidiInput::getAvailableDevices() / MidiOutput::getAvailableDevices() (juce_audio_devices,
        ALSA backend on Linux). Never hardcodes a device name; findInputContaining()/
        findOutputContaining() are a convenience for the real UI's default-select-the-XW-P1
        behaviour ("CASIO" substring), not a replacement for letting the user choose. Safe to call
        with zero devices present (headless CI). */
    namespace MidiDevices
    {
        std::vector<juce::MidiDeviceInfo> availableInputs();
        std::vector<juce::MidiDeviceInfo> availableOutputs();

        /** First input/output device whose name contains `nameSubstring` (case-insensitive), or
            std::nullopt if none match (including when there are zero devices). */
        std::optional<juce::MidiDeviceInfo> findInputContaining (const juce::String& nameSubstring);
        std::optional<juce::MidiDeviceInfo> findOutputContaining (const juce::String& nameSubstring);
    }

    /** Sends/receives XW-P1 SysEx over a juce::MidiOutput/juce::MidiInput pair. GUI-less
        (juce_audio_devices only — no GUI module).

        Thread-safety: JUCE calls MidiInputCallback::handleIncomingMidiMessage() on its own
        high-priority MIDI thread, never the message thread (see JUCE's own docs on
        MidiInputCallback). This class does the minimum possible work on that thread — copy the
        raw bytes into a queue — and returns immediately; the app drains the queue later by
        calling drainReceived() from whatever thread suits it (message thread, timer callback,
        etc). The queue is a std::deque<SysExFrame> guarded by a plain std::mutex, not a
        lock-free ring buffer: XW-P1 SysEx traffic is inherently low-frequency (one message per
        user edit, or a slow param-by-param sync poll — see reference/PROTOCOL.md §7, there is no
        bulk dump), so lock contention is a non-issue, and a mutex is far easier to reason about
        and unit-test than a lock-free structure would be. */
    class MidiIO : public juce::MidiInputCallback
    {
    public:
        MidiIO() = default;
        ~MidiIO() override;

        MidiIO (const MidiIO&) = delete;
        MidiIO& operator= (const MidiIO&) = delete;

        // ---- Output ----------------------------------------------------------------------

        /** Opens a MIDI output port by identifier (from MidiDevices::availableOutputs()).
            Returns false if the device could not be opened. */
        bool openOutput (const juce::String& deviceIdentifier);
        void closeOutput();
        bool isOutputOpen() const noexcept { return output != nullptr; }

        /** Sends a full SysExCodec-built frame (F0 ... F7 inclusive) immediately. Returns false
            if no output is open, or the frame is not well-formed (must start 0xF0, end 0xF7, and
            have at least one byte between them). */
        bool sendFrame (const SysExFrame& frame);

        // ---- Channel voice (sequencer note playback) -----------------------------------------

        /** Sends Note On (status 0x9n). channel is 1-16, note/velocity 0-127 — the caller's
            responsibility to keep in range (UI-facing values are already clamped at the widget).
            Returns false if no output is open. */
        bool sendNoteOn (int channel, int note, int velocity);

        /** Sends Note Off (status 0x8n). Same range contract as sendNoteOn(). */
        bool sendNoteOff (int channel, int note);

        /** Sends CC 123 (All Notes Off) on the given channel — a safety net for sequencer
            stop/teardown so a dropped note-off can never leave a note hanging. */
        bool sendAllNotesOff (int channel);

        /** Sends any pre-built juce::MidiMessage immediately (channel voice, SysEx, CC, NRPN, ...).
            The transport layer builds a p-lock's messages once (SysEx today, NRPN/CC later) and
            routes them either through here (audition / stop-reset) or through the timestamped
            scheduleBlock() path — one message-building seam, two delivery paths. Returns false if no
            output is open. */
        bool sendMessageNow (const juce::MidiMessage& message);

        // ---- Timestamped playback (sequencer look-ahead scheduler) ---------------------------

        /** True while a sequence is playing — i.e. the timestamped-output background thread is
            running (startPlaybackThread() called, not yet stopped). A zero-coupling "is the
            sequencer running?" signal for other panels: e.g. the tone editor suppresses its
            auto-sync while this is true, since a full-block param sync would flood the port and
            compete with the running transport. False if no output is open. */
        bool isPlaybackActive() const noexcept { return output != nullptr && output->isBackgroundThreadRunning(); }

        /** Starts JUCE's internal high-resolution output thread so scheduleBlock() can dispatch
            timestamped messages at their exact times, decoupled from the (deliberately loose,
            message-thread) look-ahead feeder — this is what keeps note timing steady even when the
            feeder timer is jittery under background load. No-op if no output is open. */
        void startPlaybackThread();

        /** Stops the output thread AND discards every still-pending scheduled message (JUCE clears
            them on stop). Because queued note-offs are dropped, the caller MUST follow this with an
            explicit all-notes-off + parameter reset. No-op if no output is open. */
        void stopPlaybackThread();

        /** Queues a block of messages for future dispatch by the output thread. Each event's sample
            position in `buffer` is converted to a wall-clock time via `samplesPerSec` against
            `startMs` (a juce::Time::getMillisecondCounter() base value). No-op if no output is open.
            Requires startPlaybackThread() to have been called. */
        void scheduleBlock (const juce::MidiBuffer& buffer, double startMs, double samplesPerSec);

        // ---- Input ------------------------------------------------------------------------

        /** Opens a MIDI input port by identifier (from MidiDevices::availableInputs()), registers
            this object as its callback, and starts it. Returns false if the device could not be
            opened. */
        bool openInput (const juce::String& deviceIdentifier);
        void closeInput();
        bool isInputOpen() const noexcept { return input != nullptr; }

        /** Pops and returns every SysEx frame received since the last call, oldest first. Safe to
            call from any thread (never call it from inside handleIncomingMidiMessage() itself). */
        std::vector<SysExFrame> drainReceived();

        /** Number of frames currently queued, awaiting drainReceived(). Mainly for tests. */
        std::size_t pendingCount() const;

        // ---- juce::MidiInputCallback --------------------------------------------------------

        /** Real JUCE callback entry point (MIDI thread). Delegates to handleRawSysEx() below so
            the exact same parsing/queueing path is exercised whether the bytes arrived from real
            hardware or were pushed directly by a test. */
        void handleIncomingMidiMessage (juce::MidiInput* source, const juce::MidiMessage& message) override;

        /** Parses and queues a raw SysEx byte buffer (F0 ... F7 inclusive) exactly as
            handleIncomingMidiMessage() does for a real callback. Public specifically so tests can
            drive the receive path with a hand-built buffer, bypassing real MIDI callback plumbing
            (no MidiInput/MidiMessage/hardware involved). Malformed input (doesn't start 0xF0 / end
            0xF7 / too short) is silently ignored, mirroring how a corrupt real-world packet would
            just fail SysExCodec::decode() downstream rather than crash. */
        void handleRawSysEx (const std::uint8_t* data, int numBytes);

        // ---- Protocol helper ----------------------------------------------------------------

        /** Builds an act=0x00 "request current value" frame

                F0 44 16 03 7F 00 <18-byte address> F7   (no value bytes)

            for the given logical paramId/instance, reusing codec.encode()'s address construction
            (act=0x01, dummy value 0) and rewriting the act byte + dropping the value bytes, rather
            than re-deriving the 18-byte address independently. This is the message syncXW fires
            once per parameter during a tone sync — the XW-P1 has no bulk/whole-tone dump
            (reference/PROTOCOL.md §7). */
        static SysExFrame syncRequest (const SysExCodec& codec, const juce::String& paramId, int instance);

    private:
        void pushReceived (SysExFrame frame);

        std::unique_ptr<juce::MidiOutput> output;
        std::unique_ptr<juce::MidiInput> input;

        mutable std::mutex queueMutex;
        std::deque<SysExFrame> received;
    };
}
