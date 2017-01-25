#include "stdafx.h"
#include "..\SharedUtilities\Logger.h"
#include "..\SharedUtilities\DMRequest.h"
#include "..\SharedUtilities\SecurityAttributes.h"
#include "CSPs\RebootCSP.h"
#include "CSPs\EnterpriseModernAppManagementCSP.h"
#include "CSPs\DeviceStatusCSP.h"
#include "CSPs\RemoteWipeCSP.h"
#include "CSPs\CustomDeviceUiCsp.h"
#include "TimeCfg.h"
#include "AppCfg.h"

#include <fstream>

#include "Models\AllModels.h"

using namespace Microsoft::Devices::Management::Message;
using namespace std;
using namespace Windows::Data::Json;

IResponse^ HandleInstallApp(IRequest^ request)
{
    try
    {
        auto appInstall = dynamic_cast<AppInstallRequest^>(request);
        auto info = appInstall->AppInstallInfo;

        std::vector<wstring> deps;
        for each (auto dep in info->Dependencies)
        {
            deps.push_back((wstring)dep->Data());
        }
        auto packageFamilyName = (wstring)info->PackageFamilyName->Data();
        auto appxPath = (wstring)info->AppxPath->Data();

        EnterpriseModernAppManagementCSP::InstallApp(packageFamilyName, appxPath, deps);
        return ref new StatusCodeResponse(ResponseStatus::Success, request->Tag);
    }
    catch (Platform::Exception^ e)
    {
        std::wstring failure(e->Message->Data());
        TRACEP(L"ERROR DMCommand::HandleInstallApp: ", Utils::ConcatString(failure.c_str(), e->HResult));
        return ref new StatusCodeResponse(ResponseStatus::Failure, request->Tag);
    }
}

IResponse^ HandleUninstallApp(IRequest^ request)
{
    try
    {
        auto appUninstall = dynamic_cast<AppUninstallRequest^>(request);
        auto info = appUninstall->AppUninstallInfo;
        auto packageFamilyName = (wstring)info->PackageFamilyName->Data();
        auto storeApp = info->StoreApp;

        EnterpriseModernAppManagementCSP::UninstallApp(packageFamilyName, storeApp);
        return ref new StatusCodeResponse(ResponseStatus::Success, request->Tag);
    }
    catch (Platform::Exception^ e)
    {
        std::wstring failure(e->Message->Data());
        TRACEP(L"ERROR DMCommand::HandleUninstallApp: ", Utils::ConcatString(failure.c_str(), e->HResult));
        return ref new StatusCodeResponse(ResponseStatus::Failure, request->Tag);
    }
}

IResponse^ HandleTransferFile(IRequest^ request)
{
    try
    {
        auto transferRequest = dynamic_cast<AzureFileTransferRequest^>(request);
        auto info = transferRequest->AzureFileTransferInfo;
        auto upload = info->Upload;
        auto localPath = (wstring)info->LocalPath->Data();
        auto appLocalDataPath = (wstring)info->AppLocalDataPath->Data();

        std::ifstream  src((upload) ? localPath : appLocalDataPath, std::ios::binary);
        std::ofstream  dst((!upload) ? localPath : appLocalDataPath, std::ios::binary);
        dst << src.rdbuf();

        return ref new StatusCodeResponse(ResponseStatus::Success, request->Tag);
    }
    catch (Platform::Exception^ e)
    {
        std::wstring failure(e->Message->Data());
        TRACEP(L"ERROR DMCommand::HandleTransferFile: ", Utils::ConcatString(failure.c_str(), e->HResult));
        return ref new StatusCodeResponse(ResponseStatus::Failure, request->Tag);
    }
}

IResponse^ HandleAppLifecycle(IRequest^ request)
{
    try
    {
        auto appLifecycle = dynamic_cast<AppLifecycleRequest^>(request);
        auto info = appLifecycle->AppLifecycleInfo;
        auto appId = (wstring)info->AppId->Data();
        bool start = info->Start;

        AppCfg::ManageApp(appId, start);
        return ref new StatusCodeResponse(ResponseStatus::Success, request->Tag);
    }
    catch (Platform::Exception^ e)
    {
        std::wstring failure(e->Message->Data());
        TRACEP(L"ERROR DMCommand::HandleAppLifecycle: ", Utils::ConcatString(failure.c_str(), e->HResult));
        return ref new StatusCodeResponse(ResponseStatus::Failure, request->Tag);
    }
}

IResponse^ HandleAddRemoveAppForStartup(StartupAppInfo^ info, DMMessageKind tag, bool add)
{
    try
    {
        auto appId = (wstring)info->AppId->Data();
        auto isBackgroundApp = info->IsBackgroundApplication;

        if (add) { CustomDeviceUiCSP::AddAsStartupApp(appId, isBackgroundApp); }
        else { CustomDeviceUiCSP::RemoveBackgroundApplicationAsStartupApp(appId); }
        return ref new StatusCodeResponse(ResponseStatus::Success, tag);
    }
    catch (Platform::Exception^ e)
    {
        std::wstring failure(e->Message->Data());
        TRACEP(L"ERROR DMCommand::HandleRemoveAppForStartup: ", Utils::ConcatString(failure.c_str(), e->HResult));
        return ref new StatusCodeResponse(ResponseStatus::Failure, tag);
    }
}

IResponse^ HandleAddAppForStartup(IRequest^ request)
{
    try
    {
        auto startupApp = dynamic_cast<AddStartupAppRequest^>(request);
        auto info = startupApp->StartupAppInfo;
        return HandleAddRemoveAppForStartup(info, request->Tag, true);
    }
    catch (Platform::Exception^ e)
    {
        std::wstring failure(e->Message->Data());
        TRACEP(L"ERROR DMCommand::HandleAddAppForStartup: ", Utils::ConcatString(failure.c_str(), e->HResult));
        return ref new StatusCodeResponse(ResponseStatus::Failure, request->Tag);
    }
}

IResponse^ HandleRemoveAppForStartup(IRequest^ request)
{
    try
    {
        auto startupApp = dynamic_cast<RemoveStartupAppRequest^>(request);
        auto info = startupApp->StartupAppInfo;
        return HandleAddRemoveAppForStartup(info, request->Tag, false);
    }
    catch (Platform::Exception^ e)
    {
        std::wstring failure(e->Message->Data());
        TRACEP(L"ERROR DMCommand::HandleRemoveAppForStartup: ", Utils::ConcatString(failure.c_str(), e->HResult));
        return ref new StatusCodeResponse(ResponseStatus::Failure, request->Tag);
    }
}

IResponse^ HandleListStartupApps(bool backgroundApps)
{
    TRACEP(L"DMCommand::HandleListStartupApps backgroundApps=", backgroundApps);
    if (backgroundApps)
    {
        auto json = CustomDeviceUiCSP::GetBackgroundTasksToLaunch();
        auto jsonArray = JsonArray::Parse(ref new Platform::String(json.c_str()));
        return ref new ListStartupBackgroundAppsResponse(ResponseStatus::Success, jsonArray);
    }
    else
    {
        auto appId = CustomDeviceUiCSP::GetStartupAppId();
        return ref new GetStartupForegroundAppResponse(ResponseStatus::Success, ref new Platform::String(appId.c_str()));
    }
}

IResponse^ HandleListApps()
{
    TRACE(__FUNCTION__);
    auto json = EnterpriseModernAppManagementCSP::GetInstalledApps();
    auto jsonMap = JsonObject::Parse(ref new Platform::String(json.c_str()));
    return ref new ListAppsResponse(ResponseStatus::Success, jsonMap);
}

#if 0 // Not yet implemented
void ProcessCommand(DMMessage& request, DMMessage& response)
{
    TRACE(__FUNCTION__);
    static int cmdIndex = 0;
    response.SetData(L"Default System Configurator Response.");

    auto command = (DMCommand)request.GetContext();
    switch (command)
    {
    case DMCommand::SetRebootInfo:
        {
            TRACE(L"SetRebootInfo:");
            try
            {
                RebootCSP::SetRebootInfo(request.GetDataW());
                response.SetContext(DMStatus::Succeeded);
            }
            catch (DMException& e)
            {
                response.SetData(e.what(), strlen(e.what()));
                response.SetContext(DMStatus::Failed);
            }
        }
        break;
    case DMCommand::GetRebootInfo:
    {
        try
        {
            TRACE(L"GetRebootInfo:");
            wstring rebootInfoJson = RebootCSP::GetRebootInfoJson();
            TRACEP(L" get json reboot info = ", rebootInfoJson.c_str());
            response.SetData(rebootInfoJson);
            response.SetContext(DMStatus::Succeeded);
        }
        catch (DMException& e)
        {
            response.SetData(e.what(), strlen(e.what()));
            response.SetContext(DMStatus::Failed);
        }
    }
    break;
    case DMCommand::FactoryReset:
        RemoteWipeCSP::DoWipe();
        response.SetData(Utils::ConcatString(L"Handling `factory reset`. cmdIndex = ", cmdIndex));
        response.SetContext(DMStatus::Succeeded);
        break;
    case DMCommand::SetTimeInfo:
        {
            try
            {
                TimeCfg::SetTimeInfo(request.GetDataW());
                response.SetContext(DMStatus::Succeeded);
            }
            catch (DMException& e)
            {
                response.SetData(e.what(), strlen(e.what()));
                response.SetContext(DMStatus::Failed);
            }
        }
        break;
    case DMCommand::GetDeviceStatus:
    {
        try
        {
            wstring deviceStatusJson = DeviceStatusCSP::GetDeviceStatusJson();
            response.SetData(deviceStatusJson);
            response.SetContext(DMStatus::Succeeded);
        }
        catch (DMException& e)
        {
            response.SetData(e.what(), strlen(e.what()));
            response.SetContext(DMStatus::Failed);
        }
    }
    break;
    default:
        response.SetData(Utils::ConcatString(L"Cannot handle unknown command...cmdIndex = ", cmdIndex));
        response.SetContext(DMStatus::Failed);
        break;
    }

    cmdIndex++;
}

#endif

// Get request and produce a response
IResponse^ ProcessCommand(IRequest^ request)
{
    TRACE(__FUNCTION__);

    switch (request->Tag)
    {
    case DMMessageKind::ListApps:
        return HandleListApps();
    case DMMessageKind::InstallApp:
        return HandleInstallApp(request);
    case DMMessageKind::UninstallApp:
        return HandleUninstallApp(request);
    case DMMessageKind::GetStartupForegroundApp:
        return HandleListStartupApps(false);
    case DMMessageKind::ListStartupBackgroundApps:
        return HandleListStartupApps(true);
    case DMMessageKind::AddStartupApp:
        return HandleAddAppForStartup(request);
    case DMMessageKind::RemoveStartupApp:
        return HandleRemoveAppForStartup(request);
    case DMMessageKind::StartApp:
    case DMMessageKind::StopApp:
        return HandleAppLifecycle(request);
    case DMMessageKind::TransferFile:
        return HandleTransferFile(request);
    case DMMessageKind::CheckUpdates:
        return ref new CheckForUpdatesResponse(ResponseStatus::Success, true);
    case DMMessageKind::RebootSystem:
        RebootCSP::ExecRebootNow();
        return ref new StatusCodeResponse(ResponseStatus::Success, DMMessageKind::RebootSystem);
    case DMMessageKind::GetTimeInfo:
        return TimeCfg::GetTimeInfo();
    default:
        TRACEP(L"Error: ", Utils::ConcatString(L"Unknown command: ", (uint32_t)request->Tag));
        throw DMException("Error: Unknown command");
    }
}

class PipeConnection
{
public:

    PipeConnection() :
        _pipeHandle(NULL)
    {}

    void Connect(HANDLE pipeHandle)
    {
        TRACE("Connecting to pipe...");
        if (pipeHandle == NULL || pipeHandle == INVALID_HANDLE_VALUE)
        {
            throw DMException("Error: Cannot connect using an invalid pipe handle.");
        }
        if (!ConnectNamedPipe(pipeHandle, NULL))
        {
            throw DMExceptionWithErrorCode("ConnectNamedPipe Error", GetLastError());
        }
        _pipeHandle = pipeHandle;
    }

    ~PipeConnection()
    {
        if (_pipeHandle != NULL)
        {
            TRACE("Disconnecting from pipe...");
            DisconnectNamedPipe(_pipeHandle);
        }
    }
private:
    HANDLE _pipeHandle;
};

void Listen()
{
    TRACE(__FUNCTION__);

    SecurityAttributes sa(GENERIC_WRITE | GENERIC_READ);

    TRACE("Creating pipe...");
    Utils::AutoCloseHandle pipeHandle = CreateNamedPipeW(
        PipeName,
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
        PIPE_UNLIMITED_INSTANCES,
        PipeBufferSize,
        PipeBufferSize,
        NMPWAIT_USE_DEFAULT_WAIT,
        sa.GetSA());

    if (pipeHandle.Get() == INVALID_HANDLE_VALUE)
    {
        throw DMExceptionWithErrorCode("CreateNamedPipe Error", GetLastError());
    }

    while (true)
    {
        PipeConnection pipeConnection;
        TRACE("Waiting for a client to connect...");
        pipeConnection.Connect(pipeHandle.Get());
        TRACE("Client connected...");

        auto request = Blob::ReadFromNativeHandle(pipeHandle.Get());
        TRACE("Request received...");
        TRACEP(L"    ", Utils::ConcatString(L"request tag:", (uint32_t)request->Tag));
        TRACEP(L"    ", Utils::ConcatString(L"request version:", request->Version));

        try
        {
            IResponse^ response = ProcessCommand(request->MakeIRequest());
            response->Serialize()->WriteToNativeHandle(pipeHandle.Get());
        }
        catch (const DMException&)
        {
            // TODO: figure out how to respond with an error that can be meaningfully handled.
            //       Is this problem fatal? So we could just die here...
            TRACE("DMExeption was thrown from ProcessCommand()...");
            throw;
        }

        // ToDo: How do we exit this loop gracefully?
    }
}