# XP-Mx210BL manual connection helper.
# It performs one explicit connect attempt only when the user clicks Connect.
# The driver exposes this IOCTL only to LocalSystem and elevated Administrators.

$principal = New-Object Security.Principal.WindowsPrincipal(
    [Security.Principal.WindowsIdentity]::GetCurrent()
)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    $powershell = Join-Path $env:WINDIR 'System32\WindowsPowerShell\v1.0\powershell.exe'
    $scriptPath = '"' + $PSCommandPath.Replace('"', '\"') + '"'
    Start-Process -FilePath $powershell -Verb RunAs -ArgumentList @(
        '-NoProfile', '-ExecutionPolicy', 'Bypass', '-STA', '-File', $scriptPath
    )
    exit
}

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

$source = @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using Microsoft.Win32.SafeHandles;
using System.Drawing;
using System.Windows.Forms;

internal static class XpmxControl
{
    private static readonly Guid InterfaceGuid =
        new Guid("7f44e8af-4c2b-4c86-8ef5-cf49e2b5da18");

    // CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED,
    //          FILE_READ_ACCESS | FILE_WRITE_ACCESS)
    private const uint IoctlConnect = 0x22E000;
    private const uint GenericReadWrite = 0xC0000000;
    private const uint ShareReadWrite = 0x00000003;
    private const uint OpenExisting = 3;

    [DllImport("cfgmgr32.dll", EntryPoint = "CM_Get_Device_Interface_List_SizeW",
        CharSet = CharSet.Unicode)]
    private static extern int GetInterfaceListSize(
        out uint length, ref Guid interfaceClassGuid, IntPtr deviceId, uint flags);

    [DllImport("cfgmgr32.dll", EntryPoint = "CM_Get_Device_Interface_ListW",
        CharSet = CharSet.Unicode)]
    private static extern int GetInterfaceList(
        ref Guid interfaceClassGuid, IntPtr deviceId,
        StringBuilder list, uint bufferLength, uint flags);

    [DllImport("kernel32.dll", EntryPoint = "CreateFileW",
        CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern SafeFileHandle CreateFile(
        string fileName, uint desiredAccess, uint shareMode,
        IntPtr securityAttributes, uint creationDisposition,
        uint flagsAndAttributes, IntPtr templateFile);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool DeviceIoControl(
        SafeFileHandle device, uint controlCode,
        IntPtr inputBuffer, uint inputBufferLength,
        IntPtr outputBuffer, uint outputBufferLength,
        out uint bytesReturned, IntPtr overlapped);

    public static string ConnectOnce()
    {
        uint length;
        int result = GetInterfaceListSize(out length, ref InterfaceGuid,
            IntPtr.Zero, 0);
        if (result != 0 || length < 2)
            throw new InvalidOperationException(
                "The XP-Mx210BL control interface is not present. " +
                "Make sure laptop Bluetooth is enabled and the paired HID device is started.");

        StringBuilder list = new StringBuilder(checked((int)length));
        result = GetInterfaceList(ref InterfaceGuid, IntPtr.Zero, list, length, 0);
        if (result != 0)
            throw new InvalidOperationException(
                "Windows could not enumerate the driver control interface (CONFIGRET " +
                result.ToString("X") + ").");

        string path = list.ToString().Split('\0')[0];
        if (String.IsNullOrEmpty(path))
            throw new InvalidOperationException("Windows returned an empty device interface path.");

        using (SafeFileHandle handle = CreateFile(path, GenericReadWrite,
            ShareReadWrite, IntPtr.Zero, OpenExisting, 0, IntPtr.Zero))
        {
            if (handle.IsInvalid)
                throw new Win32Exception(Marshal.GetLastWin32Error(),
                    "Could not open the XP-Mx210BL driver interface.");

            uint bytesReturned;
            if (!DeviceIoControl(handle, IoctlConnect,
                IntPtr.Zero, 0, IntPtr.Zero, 0, out bytesReturned, IntPtr.Zero))
            {
                throw new Win32Exception(Marshal.GetLastWin32Error(),
                    "The driver's one-shot connection attempt failed.");
            }
        }

        return "Both HID channels opened and the input reader is active. Verify input in joy.cpl or DebugView.";
    }
}

internal sealed class XpmxConnectForm : Form
{
    private readonly Button connectButton;
    private readonly Label statusLabel;

    public XpmxConnectForm()
    {
        Text = "XP-Mx210BL — Bluetooth connection";
        StartPosition = FormStartPosition.CenterScreen;
        FormBorderStyle = FormBorderStyle.FixedDialog;
        MaximizeBox = false;
        MinimizeBox = true;
        ClientSize = new Size(440, 190);
        Font = new Font("Segoe UI", 9F);

        Label heading = new Label();
        heading.Text = "Connect XP-Mx210BL";
        heading.Font = new Font("Segoe UI", 13F, FontStyle.Bold);
        heading.AutoSize = true;
        heading.Location = new Point(20, 18);
        Controls.Add(heading);

        Label instruction = new Label();
        instruction.Text = "Turn the controller on in HOME + X mode, then click Connect.";
        instruction.AutoSize = true;
        instruction.Location = new Point(22, 55);
        Controls.Add(instruction);

        connectButton = new Button();
        connectButton.Text = "Connect";
        connectButton.Size = new Size(110, 34);
        connectButton.Location = new Point(22, 88);
        connectButton.Click += ConnectClicked;
        Controls.Add(connectButton);

        statusLabel = new Label();
        statusLabel.Text = "Ready — no background retries or polling.";
        statusLabel.AutoSize = false;
        statusLabel.Size = new Size(395, 42);
        statusLabel.Location = new Point(22, 137);
        Controls.Add(statusLabel);
    }

    private async void ConnectClicked(object sender, EventArgs e)
    {
        connectButton.Enabled = false;
        statusLabel.Text = "Attempting one connection. This can take several seconds...";
        try
        {
            string result = await Task.Run(() => XpmxControl.ConnectOnce());
            statusLabel.Text = result;
        }
        catch (Exception ex)
        {
            statusLabel.Text = "Not connected: " + ex.Message;
        }
        finally
        {
            connectButton.Enabled = true;
        }
    }
}

public static class XpmxConnectApp
{
    public static void Run()
    {
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);
        Application.Run(new XpmxConnectForm());
    }
}
'@

Add-Type -TypeDefinition $source -Language CSharp
[XpmxConnectApp]::Run()
