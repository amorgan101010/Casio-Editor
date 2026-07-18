#include "casioxw/MidiIO.h"

namespace casioxw
{
    namespace MidiDevices
    {
        namespace
        {
            std::vector<juce::MidiDeviceInfo> toVector (const juce::Array<juce::MidiDeviceInfo>& arr)
            {
                std::vector<juce::MidiDeviceInfo> out;
                out.reserve ((size_t) arr.size());
                for (auto& d : arr)
                    out.push_back (d);
                return out;
            }

            std::optional<juce::MidiDeviceInfo> findContaining (const std::vector<juce::MidiDeviceInfo>& devices,
                                                                 const juce::String& nameSubstring)
            {
                for (const auto& d : devices)
                    if (d.name.containsIgnoreCase (nameSubstring))
                        return d;
                return std::nullopt;
            }
        }

        std::vector<juce::MidiDeviceInfo> availableInputs()
        {
            return toVector (juce::MidiInput::getAvailableDevices());
        }

        std::vector<juce::MidiDeviceInfo> availableOutputs()
        {
            return toVector (juce::MidiOutput::getAvailableDevices());
        }

        std::optional<juce::MidiDeviceInfo> findInputContaining (const juce::String& nameSubstring)
        {
            return findContaining (availableInputs(), nameSubstring);
        }

        std::optional<juce::MidiDeviceInfo> findOutputContaining (const juce::String& nameSubstring)
        {
            return findContaining (availableOutputs(), nameSubstring);
        }
    }

    MidiIO::~MidiIO()
    {
        // Stop the input explicitly (rather than relying on ~MidiInput()) before member
        // teardown begins, so no callback can land mid-destruction.
        closeInput();
        closeOutput();
    }

    bool MidiIO::openOutput (const juce::String& deviceIdentifier)
    {
        output = juce::MidiOutput::openDevice (deviceIdentifier);
        return output != nullptr;
    }

    void MidiIO::closeOutput()
    {
        output.reset();
    }

    bool MidiIO::sendFrame (const SysExFrame& frame)
    {
        if (output == nullptr)
            return false;
        if (frame.size() < 3 || frame.front() != 0xF0 || frame.back() != 0xF7)
            return false;

        // juce::MidiMessage::createSysExMessage() adds its own F0/F7 wrapper (and asserts the
        // payload contains neither), so pass only the bytes strictly between them.
        const auto* inner = frame.data() + 1;
        const int innerSize = (int) frame.size() - 2;
        output->sendMessageNow (juce::MidiMessage::createSysExMessage (inner, innerSize));
        return true;
    }

    bool MidiIO::sendNoteOn (int channel, int note, int velocity)
    {
        if (output == nullptr)
            return false;
        output->sendMessageNow (juce::MidiMessage::noteOn (channel, note, (juce::uint8) velocity));
        return true;
    }

    bool MidiIO::sendNoteOff (int channel, int note)
    {
        if (output == nullptr)
            return false;
        output->sendMessageNow (juce::MidiMessage::noteOff (channel, note));
        return true;
    }

    bool MidiIO::sendAllNotesOff (int channel)
    {
        if (output == nullptr)
            return false;
        output->sendMessageNow (juce::MidiMessage::controllerEvent (channel, 123, 0));
        return true;
    }

    bool MidiIO::sendMessageNow (const juce::MidiMessage& message)
    {
        if (output == nullptr)
            return false;
        output->sendMessageNow (message);
        return true;
    }

    void MidiIO::startPlaybackThread()
    {
        if (output != nullptr)
            output->startBackgroundThread();
    }

    void MidiIO::stopPlaybackThread()
    {
        if (output != nullptr)
            output->stopBackgroundThread();   // also clears any pending scheduled messages
    }

    void MidiIO::scheduleBlock (const juce::MidiBuffer& buffer, double startMs, double samplesPerSec)
    {
        if (output != nullptr)
            output->sendBlockOfMessages (buffer, startMs, samplesPerSec);
    }

    bool MidiIO::openInput (const juce::String& deviceIdentifier)
    {
        input = juce::MidiInput::openDevice (deviceIdentifier, this);
        if (input == nullptr)
            return false;
        input->start();
        return true;
    }

    void MidiIO::closeInput()
    {
        if (input != nullptr)
            input->stop();
        input.reset();
    }

    std::vector<SysExFrame> MidiIO::drainReceived()
    {
        std::lock_guard<std::mutex> lock (queueMutex);
        std::vector<SysExFrame> out (std::make_move_iterator (received.begin()),
                                     std::make_move_iterator (received.end()));
        received.clear();
        return out;
    }

    std::size_t MidiIO::pendingCount() const
    {
        std::lock_guard<std::mutex> lock (queueMutex);
        return received.size();
    }

    void MidiIO::pushReceived (SysExFrame frame)
    {
        std::lock_guard<std::mutex> lock (queueMutex);
        received.push_back (std::move (frame));
    }

    void MidiIO::handleIncomingMidiMessage (juce::MidiInput* /*source*/, const juce::MidiMessage& message)
    {
        if (message.isSysEx())
            handleRawSysEx (message.getRawData(), message.getRawDataSize());
    }

    void MidiIO::handleRawSysEx (const std::uint8_t* data, int numBytes)
    {
        if (data == nullptr || numBytes < 3 || data[0] != 0xF0 || data[(size_t) numBytes - 1] != 0xF7)
            return;   // not a well-formed SysEx frame; ignore rather than throw (MIDI-thread path)

        pushReceived (SysExFrame (data, data + numBytes));
    }

    SysExFrame MidiIO::syncRequest (const SysExCodec& codec, const juce::String& paramId, int instance)
    {
        // Reuse SysExCodec::encode()'s address construction (act=0x01 "set", dummy value 0) so
        // the 18-byte address math is never duplicated; a request frame is exactly that frame
        // with the act byte flipped to 0x00 and the value bytes dropped (reference/PROTOCOL.md §1).
        const auto setFrame = codec.encode (paramId, instance, 0);

        // Header(6) + 18-byte address = first 24 bytes; act byte is index 5.
        SysExFrame req (setFrame.begin(), setFrame.begin() + 24);
        req[5] = 0x00;
        req.push_back (0xF7);
        return req;
    }
}
