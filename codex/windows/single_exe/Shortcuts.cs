using System;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;

namespace SuperRocket64 {
    internal static class LauncherShortcuts {
        internal static string StablePath(string root) { return Path.Combine(Path.GetFullPath(root), "Super-Rocket-64.exe"); }
        internal static string EnsureStable(string root) {
            string path = StablePath(root), record = Path.Combine(Path.GetFullPath(root), "launcher-entry.json");
            Guard.NoRedirect(path); Guard.NoRedirect(record);
            if (File.Exists(record)) {
                var entry = UpdateJson.Parse(UpdateJson.ReadFile(record));
                Guard.Need(UpdateJson.Number(entry, "schema") == 1, "Unsupported launcher entry format");
                UpdateStore.Verify(path, UpdateJson.Number(entry, "size"), UpdateJson.Hash(entry, "sha256"));
                return path;
            }
            string current = UpdateStore.CurrentExe; string hash = Guard.HashFile(current); long size = new FileInfo(current).Length;
            Directory.CreateDirectory(Path.GetFullPath(root)); Guard.NoRedirect(root);
            if (!File.Exists(path)) {
                string stage = path + ".part-" + Guid.NewGuid().ToString("N");
                try { File.Copy(current, stage, false); UpdateStore.Verify(stage, size, hash); File.Move(stage, path); }
                finally { Guard.NoRedirect(stage); if (File.Exists(stage)) File.Delete(stage); }
            }
            UpdateStore.Verify(path, size, hash);
            UpdateJson.Atomic(record, UpdateJson.Bytes(new { schema = 1, size = size, sha256 = hash }));
            return path;
        }
        private static object Property(object value, string name) { return value.GetType().InvokeMember(name, BindingFlags.GetProperty, null, value, null); }
        private static void Set(object value, string name, object item) { value.GetType().InvokeMember(name, BindingFlags.SetProperty, null, value, new object[] { item }); }
        internal static string[] Read(string path) {
            Guard.NoRedirect(path); object shell = null, link = null;
            try {
                shell = Activator.CreateInstance(Type.GetTypeFromProgID("WScript.Shell", true));
                link = shell.GetType().InvokeMember("CreateShortcut", BindingFlags.InvokeMethod, null, shell, new object[] { path });
                return new string[] { (string)Property(link, "TargetPath"), (string)Property(link, "Arguments"), (string)Property(link, "WorkingDirectory") };
            } finally { if (link != null) Marshal.FinalReleaseComObject(link); if (shell != null) Marshal.FinalReleaseComObject(shell); }
        }
        private static void CheckOwned(string path, string target, string args) {
            if (!File.Exists(path)) return;
            string[] existing = Read(path);
            Guard.Need(String.Equals(existing[0], target, StringComparison.OrdinalIgnoreCase) && existing[1] == args,
                "An unrelated shortcut already uses this name. Rename that shortcut or choose a different shortcut option: " + path);
        }
        private static void Write(string path, string target, string args, string root) {
            Guard.NoRedirect(path); CheckOwned(path, target, args);
            Directory.CreateDirectory(Path.GetDirectoryName(path)); Guard.NoRedirect(path);
            string stage = Path.Combine(Path.GetDirectoryName(path), "Super-Rocket-64-" + Guid.NewGuid().ToString("N") + ".lnk");
            object shell = null, link = null;
            try {
                shell = Activator.CreateInstance(Type.GetTypeFromProgID("WScript.Shell", true));
                link = shell.GetType().InvokeMember("CreateShortcut", BindingFlags.InvokeMethod, null, shell, new object[] { stage });
                Set(link, "TargetPath", target); Set(link, "Arguments", args); Set(link, "WorkingDirectory", root);
                Set(link, "Description", "Super Rocket 64"); Set(link, "IconLocation", target + ",0");
                link.GetType().InvokeMember("Save", BindingFlags.InvokeMethod, null, link, null);
                string[] actual = Read(stage);
                Guard.Need(String.Equals(actual[0], target, StringComparison.OrdinalIgnoreCase) && actual[1] == args && String.Equals(actual[2], root, StringComparison.OrdinalIgnoreCase), "Shortcut did not retain its installation target");
                if (File.Exists(path)) File.Replace(stage, path, null); else File.Move(stage, path);
            } finally { if (link != null) Marshal.FinalReleaseComObject(link); if (shell != null) Marshal.FinalReleaseComObject(shell); Guard.NoRedirect(stage); if (File.Exists(stage)) File.Delete(stage); }
        }
        internal static void Apply(string root, bool desktop, bool startMenu, string desktopDirectory, string programsDirectory) {
            root = Path.GetFullPath(root); string target = StablePath(root), args = "--install-dir " + Commands.Quote(root);
            string desktopLink = Path.Combine(desktopDirectory, "Super Rocket 64.lnk"), menuLink = Path.Combine(programsDirectory, "Super Rocket 64", "Super Rocket 64.lnk");
            // Preflight both names before making either change. Never overwrite unrelated links.
            CheckOwned(desktopLink, target, args); CheckOwned(menuLink, target, args);
            if (desktop || startMenu) EnsureStable(root);
            if (desktop) Write(desktopLink, target, args, root); else if (File.Exists(desktopLink)) { Guard.NoRedirect(desktopLink); File.Delete(desktopLink); }
            if (startMenu) Write(menuLink, target, args, root); else if (File.Exists(menuLink)) { Guard.NoRedirect(menuLink); File.Delete(menuLink); }
            string menuFolder = Path.GetDirectoryName(menuLink); Guard.NoRedirect(menuFolder);
            if (!startMenu && Directory.Exists(menuFolder) && Directory.GetFileSystemEntries(menuFolder).Length == 0) Directory.Delete(menuFolder);
        }
        internal static void Apply(string root, bool desktop, bool startMenu) {
            Apply(root, desktop, startMenu, Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory), Environment.GetFolderPath(Environment.SpecialFolder.Programs));
        }
    }
}
