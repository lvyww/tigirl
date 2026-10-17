using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;

// A real, interactive Inno wizard on a desktop that is never switched on screen.
// A job owns the loader and its children, including on timeout/assertion failure.
public static class SetupWizardProbe
{
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    struct Startup {
        public int cb; public string reserved, desktop, title;
        public int x, y, cx, cy, charsX, charsY, fill, flags;
        public short show, bytes; public IntPtr extra, input, output, error;
    }
    [StructLayout(LayoutKind.Sequential)]
    struct ProcessInfo { public IntPtr process, thread; public uint pid, tid; }
    [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    static extern IntPtr CreateDesktop(string name, IntPtr device, IntPtr mode, uint flags, uint access, IntPtr security);
    [DllImport("user32.dll")] static extern bool CloseDesktop(IntPtr desktop);
    [DllImport("user32.dll", SetLastError = true)] static extern bool SetThreadDesktop(IntPtr desktop);
    [DllImport("user32.dll")] static extern IntPtr GetThreadDesktop(uint thread);
    [DllImport("kernel32.dll")] static extern uint GetCurrentThreadId();
    [DllImport("user32.dll")] static extern bool IsWindowEnabled(IntPtr window);
    [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr window);
    [DllImport("user32.dll", SetLastError = true)]
    static extern bool PostMessage(IntPtr window, uint message, IntPtr w, IntPtr l);
    [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    static extern IntPtr SendMessageTimeout(IntPtr window, uint message, IntPtr w, IntPtr l, uint flags, uint timeout, out IntPtr result);
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    static extern bool CreateProcess(string app, StringBuilder command, IntPtr ps, IntPtr ts, bool inherit, uint flags, IntPtr env, string directory, ref Startup startup, out ProcessInfo process);
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    static extern IntPtr CreateJobObject(IntPtr attributes, string name);
    [DllImport("kernel32.dll", SetLastError = true)] static extern bool AssignProcessToJobObject(IntPtr job, IntPtr process);
    [DllImport("kernel32.dll")] static extern bool TerminateJobObject(IntPtr job, uint code);
    [DllImport("kernel32.dll")] static extern uint ResumeThread(IntPtr thread);
    [DllImport("kernel32.dll")] static extern uint WaitForSingleObject(IntPtr handle, uint ms);
    [DllImport("kernel32.dll")] static extern bool GetExitCodeProcess(IntPtr process, out uint code);
    [DllImport("kernel32.dll")] static extern bool TerminateProcess(IntPtr process, uint code);
    [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr handle);

    static void Require(bool condition, string message) {
        if (!condition) throw new Exception(message);
    }
    static void Click(IntPtr window) {
        IntPtr result;
        Require(SendMessageTimeout(window, 0x00F5, IntPtr.Zero, IntPtr.Zero, 2, 3000, out result) != IntPtr.Zero,
            "Button did not respond to BM_CLICK");
    }
    static Dictionary<string, string> ReadReport(string path) {
        var result = new Dictionary<string, string>();
        foreach (string line in File.ReadAllLines(path)) {
            int equals = line.IndexOf('=');
            if (equals > 0) result[line.Substring(0, equals)] = line.Substring(equals + 1);
        }
        return result;
    }
    static IntPtr Handle(Dictionary<string, string> values, string name) {
        return new IntPtr(Int64.Parse(values[name]));
    }
    static void WaitFile(string path, IntPtr process) {
        var timer = Stopwatch.StartNew();
        while (!File.Exists(path)) {
            Require(WaitForSingleObject(process, 0) != 0, "Fixture exited before " + Path.GetFileName(path));
            Require(timer.ElapsedMilliseconds < 30000, "Timed out waiting for " + path);
            Thread.Sleep(25);
        }
    }
    public static void Run(string exe, string directory, string scenario, bool repaired, bool canLaunch, bool uncheck) {
        Exception failure = null;
        // HWND operations require the caller to be attached to the window's desktop.
        // Use a fresh thread: the PowerShell host may already own windows/hooks.
        var thread = new Thread(delegate() {
            try { RunOnDesktop(exe, directory, scenario, repaired, canLaunch, uncheck); }
            catch (Exception error) { failure = error; }
        });
        thread.Start(); thread.Join();
        if (failure != null) throw failure;
    }
    static void RunOnDesktop(string exe, string directory, string scenario, bool repaired, bool canLaunch, bool uncheck) {
        IntPtr previousDesktop = GetThreadDesktop(GetCurrentThreadId());
        string name = "TigirlSetupProbe_" + Guid.NewGuid().ToString("N");
        IntPtr desktop = CreateDesktop(name, IntPtr.Zero, IntPtr.Zero, 0, 0x01ff, IntPtr.Zero);
        if (desktop == IntPtr.Zero) throw new Win32Exception();
        IntPtr job = IntPtr.Zero;
        ProcessInfo process = new ProcessInfo();
        try {
            if (!SetThreadDesktop(desktop)) throw new Win32Exception();
            job = CreateJobObject(IntPtr.Zero, null);
            if (job == IntPtr.Zero) throw new Win32Exception();
            var startup = new Startup(); startup.cb = Marshal.SizeOf(typeof(Startup)); startup.desktop = name;
            var command = new StringBuilder("\"" + exe + "\" /SP- /SUPPRESSMSGBOXES /NORESTART /Output=\"" + directory +
                "\" /Scenario=" + scenario + " /LOG=\"" + Path.Combine(directory, "setup.log") + "\"");
            if (!CreateProcess(exe, command, IntPtr.Zero, IntPtr.Zero, false, 4, IntPtr.Zero,
                Path.GetDirectoryName(exe), ref startup, out process)) throw new Win32Exception();
            if (!AssignProcessToJobObject(job, process.process)) throw new Win32Exception();
            Require(ResumeThread(process.thread) != UInt32.MaxValue, "Resume fixture failed");
            WaitFile(Path.Combine(directory, "ready.txt"), process.process);
            var report = ReadReport(Path.Combine(directory, "ready.txt"));
            IntPtr wizard = Handle(report, "wizard");
            Require(IsWindowVisible(wizard), "Fixture wizard is not visible on isolated desktop");
            if (!repaired) {
                Require(!IsWindowEnabled(wizard), "Negative control did not reproduce disabled finished page");
                Require(report["native_before"] == "0" && report["vcl_before"] == "1", "Native/VCL mismatch was not reproduced");
                return;
            }
            Require(IsWindowEnabled(wizard), "Finished wizard remained disabled");
            IntPtr finish = Handle(report, "finish");
            Require(IsWindowEnabled(finish) && IsWindowVisible(finish), "Finish button is unusable");
            Require((report["run_count"] == "1") == canLaunch, "Wrong settings launch availability");
            Click(Handle(report, "log"));
            WaitFile(Path.Combine(directory, "log-clicked.txt"), process.process);
            Require(IsWindowEnabled(wizard), "Log button disabled finished wizard");
            if (canLaunch && uncheck) {
                IntPtr list = Handle(report, "run_list");
                // First row/check box, using its actual DPI-scaled item height.
                int xy = (Int32.Parse(report["run_height"]) / 2 << 16) | 12;
                Require(PostMessage(list, 0x0201, new IntPtr(1), new IntPtr(xy)), "Check box down failed");
                Require(PostMessage(list, 0x0202, IntPtr.Zero, new IntPtr(xy)), "Check box up failed");
            }
            // Post in the same thread's queue, after the checkbox mouse messages.
            Require(PostMessage(finish, 0x00F5, IntPtr.Zero, IntPtr.Zero), "Finish click failed");
            Require(WaitForSingleObject(process.process, 15000) == 0, "Finish did not close installer");
            uint code;
            Require(GetExitCodeProcess(process.process, out code) && code == 0, "Fixture failed to exit cleanly");
            Require(File.Exists(Path.Combine(directory, "finish-clicked.txt")), "Finish handler did not run");
            if (canLaunch && !uncheck) {
                var timer = Stopwatch.StartNew();
                while (!File.Exists(Path.Combine(directory, "settings-launched.txt")) && timer.ElapsedMilliseconds < 10000) Thread.Sleep(25);
                Require(File.Exists(Path.Combine(directory, "settings-launched.txt")), "Checked postinstall action did not run");
            } else {
                Require(!File.Exists(Path.Combine(directory, "settings-launched.txt")), "Disabled/unchecked postinstall action ran");
                if (canLaunch) Require(File.ReadAllText(Path.Combine(directory, "finish-clicked.txt")).Trim() == "0", "Checkbox mouse click did not uncheck launch");
            }
        } finally {
            if (job != IntPtr.Zero) { TerminateJobObject(job, 99); CloseHandle(job); }
            if (process.process != IntPtr.Zero) {
                if (WaitForSingleObject(process.process, 1000) != 0) TerminateProcess(process.process, 99);
                CloseHandle(process.process);
            }
            if (process.thread != IntPtr.Zero) CloseHandle(process.thread);
            SetThreadDesktop(previousDesktop);
            CloseDesktop(desktop);
        }
    }
}
