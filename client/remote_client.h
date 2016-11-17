/*
* PROJECT:         Aspia Remote Desktop
* FILE:            client/remote_client.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_CLIENT__REMOTE_CLIENT_H
#define _ASPIA_CLIENT__REMOTE_CLIENT_H

#include "aspia_config.h"

#include "base/macros.h"
#include "base/thread.h"
#include "base/mutex.h"
#include "network/socket_tcp.h"
#include "codec/video_decoder_raw.h"
#include "codec/video_decoder_zlib.h"
#include "codec/video_decoder_vp8.h"
#include "client/screen_window_win.h"
#include "client/message_queue.h"

class RemoteClient : private Thread
{
public:
    enum class EventType
    {
        Connected,    // ������� ��������� � �������.
        Disconnected, // �������� �� �������.
        BadAuth,      // �� ������� ������ �����������.
        NotConnected  // �� ������� ������������.
    };

    typedef std::function<void(EventType)> OnEventCallback;

    RemoteClient(OnEventCallback on_event);
    ~RemoteClient();

    void ConnectTo(const char *hostname, int port);
    void Disconnect();

    void SendPointerEvent(int32_t x, int32_t y, int32_t mask);
    void SendKeyEvent(int32_t keycode, bool extended, bool pressed);

    void ProcessMessage(const proto::ServerToClient *message);

private:
    void Worker() override;
    void OnStart() override;
    void OnStop() override;

    void OnScreenWindowClosed();

    void SendAuthReply(int32_t method);

    void SendVideoControl(bool enable,
                          int32_t encoding,
                          const PixelFormat &pixel_format = PixelFormat::MakeRGB565());

    int32_t ReadAuthRequest();
    void ReadAuthResult(int32_t feature);
    void ReadVideoPacket(const proto::VideoPacket &packet);

private:
    std::string hostname_;
    int port_;

    OnEventCallback on_event_;

    std::unique_ptr<Socket> socket_;
    std::unique_ptr<MessageQueue> message_queue_;
    std::unique_ptr<VideoDecoder> decoder_;
    std::unique_ptr<ScreenWindowWin> window_;

    int32_t current_encoding_;

    DesktopSize screen_size_;
    PixelFormat pixel_format_;

    DISALLOW_COPY_AND_ASSIGN(RemoteClient);
};

#endif // _ASPIA_CLIENT__REMOTE_CLIENT_H
