using System;
using System.ComponentModel;
using System.Runtime.InteropServices;

namespace NativeTiger.Tools;

// CoreRuntimeState synchronizes HKCU autorun even with a differential filesystem
// root. Redirect HKCU for this entire short-lived process before initializing it.
internal sealed class RegistrySandbox : IDisposable
{
    private static readonly IntPtr CurrentUser = new IntPtr(unchecked((int)0x80000001));
    private readonly string path = @"Software\NativeTiger\OracleSandbox\" + Guid.NewGuid().ToString("N");
    private IntPtr handle;

    [DllImport("advapi32.dll", CharSet = CharSet.Unicode)]
    private static extern int RegCreateKeyExW(IntPtr root, string subkey, int reserved, string keyClass,
        int options, int access, IntPtr security, out IntPtr result, out int disposition);
    [DllImport("advapi32.dll")]
    private static extern int RegOverridePredefKey(IntPtr predefined, IntPtr key);
    [DllImport("advapi32.dll")]
    private static extern int RegCloseKey(IntPtr key);
    [DllImport("advapi32.dll", CharSet = CharSet.Unicode)]
    private static extern int RegDeleteTreeW(IntPtr root, string subkey);

    internal RegistrySandbox()
    {
        int error = RegCreateKeyExW(CurrentUser, path, 0, null, 0, 0xF003F, IntPtr.Zero, out handle, out _);
        if (error != 0) throw new Win32Exception(error, "Cannot create oracle registry sandbox");
        error = RegOverridePredefKey(CurrentUser, handle);
        if (error != 0) {
            RegCloseKey(handle); handle = IntPtr.Zero;
            throw new Win32Exception(error, "Cannot isolate oracle registry");
        }
    }

    public void Dispose()
    {
        if (handle == IntPtr.Zero) return;
        int error = RegOverridePredefKey(CurrentUser, IntPtr.Zero);
        RegCloseKey(handle); handle = IntPtr.Zero;
        if (error != 0) throw new Win32Exception(error, "Cannot restore oracle registry mapping");
        RegDeleteTreeW(CurrentUser, path);
    }
}
