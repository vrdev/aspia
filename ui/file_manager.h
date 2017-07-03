//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/file_manager.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__FILE_MANAGER_H
#define _ASPIA_UI__FILE_MANAGER_H

#include "base/message_loop/message_loop_thread.h"
#include "proto/file_transfer_session.pb.h"
#include "ui/base/splitter.h"
#include "ui/file_manager_panel.h"

namespace aspia {

class UiFileManager :
    public CWindowImpl<UiFileManager, CWindow, CFrameWinTraits>,
    private MessageLoopThread::Delegate,
    private UiFileManagerPanel::Delegate
{
public:
    using PanelType = UiFileManagerPanel::PanelType;

    class Delegate
    {
    public:
        virtual void OnWindowClose() = 0;
        virtual void OnDriveListRequest(PanelType panel_type) = 0;

        virtual void OnDirectoryListRequest(PanelType panel_type,
                                            const std::string& path,
                                            const std::string& item) = 0;

        virtual void OnCreateDirectoryRequest(PanelType panel_type,
                                              const std::string& path,
                                              const std::string& name) = 0;

        virtual void OnRenameRequest(PanelType panel_type,
                                     const std::string& path,
                                     const std::string& old_name,
                                     const std::string& new_name) = 0;

        virtual void OnRemoveRequest(PanelType panel_type,
                                     const std::string& path,
                                     const std::string& item_name) = 0;

        virtual void OnSendFile(const std::wstring& from_path, const std::wstring& to_path) = 0;
        virtual void OnRecieveFile(const std::wstring& from_path, const std::wstring& to_path) = 0;
    };

    UiFileManager(Delegate* delegate);
    ~UiFileManager();

    void ReadRequestStatus(std::shared_ptr<proto::RequestStatus> status);

    void ReadDriveList(PanelType panel_type,
                       std::unique_ptr<proto::DriveList> drive_list);

    void ReadDirectoryList(PanelType panel_type,
                           std::unique_ptr<proto::DirectoryList> directory_list);

private:
    // MessageLoopThread::Delegate implementation.
    void OnBeforeThreadRunning() override;
    void OnAfterThreadRunning() override;

    // UiFileManagerPanel::Delegate implementation.
    void OnDriveListRequest(PanelType panel_type) override;

    void OnDirectoryListRequest(PanelType panel_type,
                                const std::string& path,
                                const std::string& item) override;

    void OnCreateDirectoryRequest(PanelType panel_type,
                                  const std::string& path,
                                  const std::string& name) override;

    void OnRenameRequest(PanelType panel_type,
                         const std::string& path,
                         const std::string& old_path,
                         const std::string& new_path) override;

    void OnRemoveRequest(PanelType panel_type,
                         const std::string& path,
                         const std::string& item_name) override;

    BEGIN_MSG_MAP(UiFileManager)
        MESSAGE_HANDLER(WM_CREATE, OnCreate)
        MESSAGE_HANDLER(WM_SIZE, OnSize)
        MESSAGE_HANDLER(WM_GETMINMAXINFO, OnGetMinMaxInfo)
        MESSAGE_HANDLER(WM_CLOSE, OnClose)
        MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
    END_MSG_MAP()

    LRESULT OnCreate(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnDestroy(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnSize(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnGetMinMaxInfo(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnClose(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);

    MessageLoopThread ui_thread_;
    std::shared_ptr<MessageLoopProxy> runner_;

    Delegate* delegate_;

    UiFileManagerPanel local_panel_;
    UiFileManagerPanel remote_panel_;
    UiVerticalSplitter splitter_;

    CIcon small_icon_;
    CIcon big_icon_;

    DISALLOW_COPY_AND_ASSIGN(UiFileManager);
};

} // namespace aspia

#endif // _ASPIA_UI__FILE_MANAGER_H
