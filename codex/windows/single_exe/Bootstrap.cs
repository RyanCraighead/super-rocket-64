// Packaging bootstrap only. The game remains the exact separately verified PE.
using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Globalization;
using System.IO;
using System.IO.Compression;
using System.Net;
using System.Net.NetworkInformation;
using System.Net.Sockets;
using System.Reflection;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Web.Script.Serialization;
using System.Windows.Forms;

namespace SuperRocket64 {
    internal static class Guard {
        internal static void Need(bool condition, string message) {
            if (!condition) throw new InvalidDataException(message);
        }
        internal static string Hash(Stream stream) {
            using (SHA256 hash = SHA256.Create())
                return BitConverter.ToString(hash.ComputeHash(stream)).Replace("-", "").ToLowerInvariant();
        }
        internal static string HashFile(string file) {
            using (FileStream stream = new FileStream(file, FileMode.Open, FileAccess.Read, FileShare.Read)) return Hash(stream);
        }
        internal static void NoRedirect(string path) {
            string current = Path.GetFullPath(path);
            while (!String.IsNullOrEmpty(current)) {
                try {
                    Need((File.GetAttributes(current) & FileAttributes.ReparsePoint) == 0, "Links/junctions are not allowed: " + current);
                } catch (FileNotFoundException) { } catch (DirectoryNotFoundException) { }
                string parent = Path.GetDirectoryName(current);
                if (parent == current) break;
                current = parent;
            }
        }
        internal static string Relative(string value) {
            Need(!String.IsNullOrEmpty(value) && value.Length <= 220 && !Path.IsPathRooted(value) &&
                 value.IndexOf('\\') < 0 && value.IndexOf(':') < 0, "Invalid payload path");
            foreach (string part in value.Split('/')) {
                Need(part.Length > 0 && part != "." && part != ".." && !part.StartsWith(".") &&
                     !part.EndsWith(".") && !part.EndsWith(" "), "Invalid payload component: " + value);
                foreach (char c in part)
                    Need(c >= 32 && c < 127 && Array.IndexOf(Path.GetInvalidFileNameChars(), c) < 0, "Unsupported payload filename");
                string stem = part.Split('.')[0].ToUpperInvariant();
                Need(!Regex.IsMatch(stem, @"^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9]|CONIN\$|CONOUT\$)$"), "Reserved Windows filename");
            }
            string extension = Path.GetExtension(value).ToLowerInvariant();
            Need(extension != ".z64" && extension != ".v64" && extension != ".n64" && extension != ".upk",
                 "Original game inputs cannot be payload files");
            return value;
        }
        internal static string Child(string root, string relative) {
            string file = Path.GetFullPath(Path.Combine(root, Relative(relative).Replace('/', Path.DirectorySeparatorChar)));
            Need(file.StartsWith(root.TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase), "Payload escaped destination");
            NoRedirect(file);
            return file;
        }
    }

    internal sealed class FileRecord {
        internal long Size;
        internal string Sha;
    }

    internal static class Installer {
        internal const long MaxZip = 512L * 1024 * 1024;
        internal const long MaxExpanded = 1024L * 1024 * 1024;
        internal static string Destination(string installBase, string sha) {
            Guard.Need(Regex.IsMatch(sha ?? "", "^[0-9a-f]{64}$"), "Invalid embedded payload hash");
            Guard.Need(!String.IsNullOrWhiteSpace(installBase), "Choose an installation folder");
            string root = Path.GetFullPath(Path.Combine(installBase, sha.Substring(0, 16)));
            Guard.Need(!root.StartsWith(@"\\", StringComparison.Ordinal) && new DriveInfo(Path.GetPathRoot(root)).DriveType != DriveType.Network,
                       "Choose a local installation drive; network destinations are not supported");
            Guard.Need(root.Length <= 100, "Choose a shorter installation folder (final package path must be at most 100 characters)");
            foreach (char c in root) Guard.Need(c >= 32 && c < 127, "Choose an ASCII-only installation folder; the game's file loader requires it");
            Guard.NoRedirect(root);
            return root;
        }
        internal static Dictionary<string, FileRecord> Manifest(byte[] bytes) {
            Guard.Need(bytes.Length > 0 && bytes.Length <= 4 * 1024 * 1024, "Invalid package manifest size");
            JavaScriptSerializer json = new JavaScriptSerializer { MaxJsonLength = 4 * 1024 * 1024, RecursionLimit = 32 };
            Dictionary<string, object> document = json.DeserializeObject(new UTF8Encoding(false, true).GetString(bytes)) as Dictionary<string, object>;
            Guard.Need(document != null && document.ContainsKey("files"), "Missing manifest files map");
            Dictionary<string, object> records = document["files"] as Dictionary<string, object>;
            Guard.Need(records != null && records.Count > 0 && records.Count <= 5000, "Invalid manifest file count");
            Dictionary<string, FileRecord> result = new Dictionary<string, FileRecord>(StringComparer.OrdinalIgnoreCase);
            long total = 0;
            foreach (KeyValuePair<string, object> item in records) {
                string name = Guard.Relative(item.Key);
                Guard.Need(!name.Equals("PACKAGE-MANIFEST.json", StringComparison.OrdinalIgnoreCase) && !result.ContainsKey(name), "Duplicate/self-referential manifest path");
                Dictionary<string, object> record = item.Value as Dictionary<string, object>;
                Guard.Need(record != null && record.ContainsKey("size") && record.ContainsKey("sha256"), "Malformed file record");
                object sizeObject = record["size"];
                Guard.Need(sizeObject is int || sizeObject is long, "File size must be an integer");
                long size = Convert.ToInt64(sizeObject, CultureInfo.InvariantCulture);
                string sha = record["sha256"] as string;
                Guard.Need(size >= 0 && size <= 256L * 1024 * 1024 && Regex.IsMatch(sha ?? "", "^[0-9a-f]{64}$"), "Invalid file size/hash");
                total += size;
                Guard.Need(total <= MaxExpanded, "Payload expands beyond the allowed size");
                result.Add(name, new FileRecord { Size = size, Sha = sha });
            }
            foreach (string name in result.Keys) {
                string prefix = name;
                while (prefix.LastIndexOf('/') >= 0) {
                    prefix = prefix.Substring(0, prefix.LastIndexOf('/'));
                    Guard.Need(!result.ContainsKey(prefix), "Manifest file is also a directory");
                }
            }
            foreach (string required in new string[] { "sm64coopdx.exe", "python/python.exe", "codex/windows/seven_launcher.py", "lang/English.ini" })
                Guard.Need(result.ContainsKey(required), "Required payload file missing: " + required);
            return result;
        }
        private static byte[] ReadSmall(ZipArchiveEntry entry) {
            Guard.Need(entry.Length <= 4 * 1024 * 1024, "Manifest is too large");
            using (Stream source = entry.Open()) using (MemoryStream destination = new MemoryStream()) {
                source.CopyTo(destination);
                Guard.Need(destination.Length == entry.Length, "Manifest length changed");
                return destination.ToArray();
            }
        }
        private static void VerifyFile(string path, FileRecord record) {
            Guard.NoRedirect(path);
            Guard.Need(File.Exists(path) && new FileInfo(path).Length == record.Size && Guard.HashFile(path) == record.Sha,
                       "Installed file missing or changed: " + path + ". Choose a fresh installation folder; private data will not be overwritten.");
        }
        private static void VerifyTree(string directory, string root, Dictionary<string, FileRecord> files) {
            foreach (string path in Directory.GetFileSystemEntries(directory)) {
                Guard.NoRedirect(path);
                string relative = path.Substring(root.Length + 1).Replace('\\', '/');
                // Mutable setup/game data is private and never part of the package inventory.
                if (relative.Equals(".runtime", StringComparison.OrdinalIgnoreCase)) {
                    Guard.Need(Directory.Exists(path), "Private runtime must be a directory");
                    continue;
                }
                if (Directory.Exists(path)) VerifyTree(path, root, files);
                else Guard.Need(relative == "PACKAGE-MANIFEST.json" || files.ContainsKey(relative), "Unexpected file in immutable package: " + relative);
            }
        }
        private static void VerifyInstalled(string root, Dictionary<string, FileRecord> files, string manifestHash) {
            Guard.NoRedirect(root);
            Guard.Need(Directory.Exists(root), "Package root is not a directory");
            Guard.Need(Guard.HashFile(Guard.Child(root, "PACKAGE-MANIFEST.json")) == manifestHash, "Installed manifest differs; refusing overwrite");
            foreach (KeyValuePair<string, FileRecord> item in files) VerifyFile(Guard.Child(root, item.Key), item.Value);
            VerifyTree(root, root, files);
        }
        internal static string Install(Stream payload, string expectedHash, long expectedSize, string installBase, Action<string> log) {
            string root = Destination(installBase, expectedHash);
            Guard.Need(payload != null && payload.CanSeek && expectedSize > 0 && expectedSize <= MaxZip && payload.Length == expectedSize, "Embedded ZIP size mismatch");
            payload.Position = 0;
            Guard.Need(Guard.Hash(payload) == expectedHash, "Embedded ZIP hash mismatch");
            payload.Position = 0;
            using (ZipArchive zip = new ZipArchive(payload, ZipArchiveMode.Read, true)) {
                Guard.Need(zip.Entries.Count > 0 && zip.Entries.Count <= 6000, "Invalid ZIP entry count");
                Dictionary<string, ZipArchiveEntry> entries = new Dictionary<string, ZipArchiveEntry>(StringComparer.OrdinalIgnoreCase);
                foreach (ZipArchiveEntry entry in zip.Entries) {
                    string name = entry.FullName.EndsWith("/") ? entry.FullName.Substring(0, entry.FullName.Length - 1) : entry.FullName;
                    Guard.Relative(name);
                    int fileType = (entry.ExternalAttributes >> 16) & 0xf000;
                    Guard.Need(fileType == 0 || fileType == 0x8000 || fileType == 0x4000, "Non-regular ZIP entry");
                    Guard.Need((entry.ExternalAttributes & (int)FileAttributes.ReparsePoint) == 0, "ZIP reparse point");
                    Guard.Need(!entries.ContainsKey(name), "Duplicate/case-colliding ZIP path");
                    entries.Add(name, entry);
                }
                Guard.Need(entries.ContainsKey("PACKAGE-MANIFEST.json"), "ZIP lacks package manifest");
                byte[] manifestBytes = ReadSmall(entries["PACKAGE-MANIFEST.json"]);
                string manifestHash;
                using (MemoryStream stream = new MemoryStream(manifestBytes)) manifestHash = Guard.Hash(stream);
                Dictionary<string, FileRecord> files = Manifest(manifestBytes);
                int fileCount = 0;
                foreach (KeyValuePair<string, ZipArchiveEntry> item in entries) {
                    if (item.Value.FullName.EndsWith("/")) {
                        Guard.Need(item.Value.Length == 0 && !files.ContainsKey(item.Key), "Invalid payload directory");
                        bool parent = false;
                        foreach (string name in files.Keys) if (name.StartsWith(item.Key + "/", StringComparison.OrdinalIgnoreCase)) parent = true;
                        Guard.Need(parent, "Unexpected payload directory");
                        continue;
                    }
                    fileCount++;
                    if (item.Key == "PACKAGE-MANIFEST.json") continue;
                    Guard.Need(files.ContainsKey(item.Key) && files[item.Key].Size == item.Value.Length, "Unmanifested file or size mismatch: " + item.Key);
                }
                Guard.Need(fileCount == files.Count + 1, "Manifest and ZIP file sets differ");
                foreach (string name in files.Keys) Guard.Need(entries.ContainsKey(name), "Manifested file absent from ZIP");
                if (File.Exists(root) || Directory.Exists(root)) {
                    VerifyInstalled(root, files, manifestHash);
                    log("Existing package verified; private profiles left unchanged.");
                    return root;
                }
                string parentRoot = Path.GetDirectoryName(root);
                Guard.NoRedirect(parentRoot);
                Directory.CreateDirectory(parentRoot);
                Guard.NoRedirect(parentRoot);
                string stage = Path.Combine(parentRoot, ".stage-" + Guid.NewGuid().ToString("N"));
                Directory.CreateDirectory(stage);
                log("Extracting verified program files to " + root);
                try {
                    foreach (KeyValuePair<string, FileRecord> item in files) {
                        string target = Guard.Child(stage, item.Key);
                        Directory.CreateDirectory(Path.GetDirectoryName(target));
                        Guard.NoRedirect(target);
                        using (Stream source = entries[item.Key].Open())
                        using (FileStream destination = new FileStream(target, FileMode.CreateNew, FileAccess.Write, FileShare.None)) {
                            byte[] buffer = new byte[65536]; int count; long copied = 0;
                            while ((count = source.Read(buffer, 0, buffer.Length)) > 0) {
                                copied += count;
                                Guard.Need(copied <= item.Value.Size, "Expanded file exceeded manifest size");
                                destination.Write(buffer, 0, count);
                            }
                            Guard.Need(copied == item.Value.Size, "Truncated payload file");
                        }
                        VerifyFile(target, item.Value);
                    }
                    using (FileStream destination = new FileStream(Guard.Child(stage, "PACKAGE-MANIFEST.json"), FileMode.CreateNew, FileAccess.Write, FileShare.None))
                        destination.Write(manifestBytes, 0, manifestBytes.Length);
                    VerifyInstalled(stage, files, manifestHash);
                    Guard.NoRedirect(root);
                    Guard.Need(!File.Exists(root) && !Directory.Exists(root), "Installation appeared during extraction; refusing overwrite");
                    Directory.Move(stage, root);
                } finally {
                    // Only this newly generated stage is ours. No recursive deletion follows redirects.
                    if (Directory.Exists(stage)) RemoveStage(stage);
                }
                log("Package verified. No game or setup tool has been started.");
                return root;
            }
        }
        private static void RemoveStage(string directory) {
            Guard.NoRedirect(directory);
            foreach (string item in Directory.GetFileSystemEntries(directory)) {
                Guard.NoRedirect(item);
                if (Directory.Exists(item)) RemoveStage(item); else File.Delete(item);
            }
            Directory.Delete(directory);
        }
        internal static void RepairEmbedded(string installBase, Action<string> log) { RepairEmbedded(installBase, log, UpdateStore.GameActive); }
        internal static void RepairEmbedded(string installBase, Action<string> log, Func<bool> gameActive) {
            Guard.Need(!gameActive(), "Close the game normally before repairing program files.");
            string root = Destination(installBase, PayloadInfo.ZipSha256);
            string stagingBase = Path.Combine(Path.GetFullPath(installBase), ".repair-" + Guid.NewGuid().ToString("N").Substring(0, 8));
            string replacement = null, backup = root + ".backup-" + Guid.NewGuid().ToString("N").Substring(0, 8);
            bool movedOld = false, movedRuntime = false;
            try {
                replacement = Embedded(stagingBase, log);
                Guard.Need(!gameActive(), "Close the game normally before repairing program files.");
                Guard.NoRedirect(root); Guard.NoRedirect(backup);
                if (Directory.Exists(root)) { Directory.Move(root, backup); movedOld = true; }
                try {
                    if (movedOld && Directory.Exists(Path.Combine(backup, ".runtime"))) {
                        Guard.NoRedirect(Path.Combine(backup, ".runtime"));
                        Directory.Move(Path.Combine(backup, ".runtime"), Path.Combine(replacement, ".runtime")); movedRuntime = true;
                    }
                    Directory.Move(replacement, root);
                } catch {
                    if (movedRuntime) Directory.Move(Path.Combine(replacement, ".runtime"), Path.Combine(backup, ".runtime"));
                    if (movedOld && !Directory.Exists(root)) Directory.Move(backup, root);
                    throw;
                }
                log("Program files verified and repaired. Previous files retained for recovery.");
            } finally {
                // Delete only our empty staging parent. Keep any interrupted
                // replacement for recovery; never recursively erase user data.
                if (Directory.Exists(stagingBase) && Directory.GetFileSystemEntries(stagingBase).Length == 0) Directory.Delete(stagingBase);
            }
        }
        internal static string Embedded(string installBase, Action<string> log) {
            using (Stream payload = Assembly.GetExecutingAssembly().GetManifestResourceStream("Payload"))
                return Install(payload, PayloadInfo.ZipSha256, PayloadInfo.ZipSize, installBase, log);
        }
    }

    internal sealed class Options {
        internal bool VerifyOnly;
        internal bool UpdateProbe;
        internal bool ListAddresses;
        internal bool CheckUpdates;
        internal string InstallBase;
        internal string ReadyToken;
        internal static Options Parse(string[] args) {
            Options options = new Options();
            for (int i = 0; i < args.Length; i++) {
                if (args[i] == "--verify-only" && !options.VerifyOnly) options.VerifyOnly = true;
                else if (args[i] == "--update-probe" && !options.UpdateProbe) options.UpdateProbe = true;
                else if (args[i] == "--update-ready" && options.ReadyToken == null && i + 1 < args.Length) options.ReadyToken = args[++i];
                else if (args[i] == "--list-addresses" && !options.ListAddresses) options.ListAddresses = true;
                else if (args[i] == "--check-updates" && !options.CheckUpdates) options.CheckUpdates = true;
                else if (args[i] == "--install-dir" && options.InstallBase == null && i + 1 < args.Length && !args[i+1].StartsWith("--")) options.InstallBase = args[++i];
                else throw new ArgumentException("Usage: Super-Rocket-64.exe [--install-dir BASE] [--verify-only] or --list-addresses");
            }
            Guard.Need(!options.ListAddresses || (!options.VerifyOnly && options.InstallBase == null), "--list-addresses cannot be combined with installation options");
            Guard.Need(!options.UpdateProbe || (!options.VerifyOnly && !options.ListAddresses && options.InstallBase != null && options.ReadyToken == null), "Invalid update probe arguments");
            Guard.Need(!options.CheckUpdates || (!options.VerifyOnly && !options.ListAddresses && !options.UpdateProbe && options.InstallBase == null && options.ReadyToken == null), "--check-updates cannot be combined with installation options");
            Guard.Need(options.ReadyToken == null || (!options.VerifyOnly && !options.ListAddresses && options.InstallBase != null && Regex.IsMatch(options.ReadyToken, "^[a-f0-9]{32}$")), "Invalid update handshake");
            if (options.VerifyOnly) Guard.Need(options.InstallBase != null, "--verify-only requires --install-dir BASE");
            if (options.InstallBase == null) options.InstallBase = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "SuperRocket64");
            return options;
        }
    }

    internal static class Commands {
        internal static readonly string[] Characters = { "mario", "link", "bomberman", "banjo", "spiderman", "tony", "octane" };
        internal static readonly string[] OptionalCharacters = { "link", "bomberman", "banjo", "spiderman", "tony" };
        private static readonly object environmentLock = new object();
        private static void NormalizeProcessEnvironmentAliases() {
            // Some invoking shells supply both PATH and Path. Framework's lazy
            // ProcessStartInfo dictionary throws before it can be cleared. Remove
            // only duplicate aliases in this bootstrap process, never registry,
            // user/machine settings, the parent process or unrelated processes.
            Dictionary<string, int> counts = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase);
            foreach (DictionaryEntry item in Environment.GetEnvironmentVariables()) {
                string name = (string)item.Key;
                counts[name] = counts.ContainsKey(name) ? counts[name] + 1 : 1;
            }
            foreach (KeyValuePair<string, int> item in counts) if (item.Value > 1) {
                string value = Environment.GetEnvironmentVariable(item.Key);
                for (int i = 0; i < item.Value + 1; i++) Environment.SetEnvironmentVariable(item.Key, null, EnvironmentVariableTarget.Process);
                Environment.SetEnvironmentVariable(item.Key, value, EnvironmentVariableTarget.Process);
            }
        }
        internal static string Quote(string value) {
            Guard.Need(value != null && value.IndexOf('\0') < 0 && value.IndexOf('\r') < 0 && value.IndexOf('\n') < 0, "Invalid process argument");
            StringBuilder result = new StringBuilder("\"");
            int slashes = 0;
            foreach (char c in value) {
                if (c == '\\') { slashes++; continue; }
                if (c == '"') { result.Append('\\', slashes * 2 + 1); result.Append(c); }
                else { result.Append('\\', slashes); result.Append(c); }
                slashes = 0;
            }
            result.Append('\\', slashes * 2); result.Append('"'); return result.ToString();
        }
        internal static List<string> Setup(string character, string rom, string characterRom, string assets, string game, bool download) {
            Guard.Need(Array.IndexOf(Characters, character) >= 0, "Unsupported setup character");
            List<string> result = new List<string> { "setup", "--character", character };
            if (!String.IsNullOrWhiteSpace(rom)) {
                Guard.Need(File.Exists(rom), "Choose your SM64 US ROM first");
                result.AddRange(new string[] { "--sm64", Path.GetFullPath(rom) });
            } // Blank inputs let the helper validate and reuse an existing profile first.
            if (character == "octane") {
                if (!String.IsNullOrWhiteSpace(game)) {
                    Guard.Need(Directory.Exists(game), "Rocket League installation folder is missing (contains TAGame)");
                    result.AddRange(new string[] { "--game", Path.GetFullPath(game) });
                }
                if (download) result.Add("--download-ueviewer");
            } else if (character != "mario") {
                if (!String.IsNullOrWhiteSpace(assets)) {
                    Guard.Need(Directory.Exists(assets), "Prepared character asset folder is missing");
                    result.AddRange(new string[] { "--assets", Path.GetFullPath(assets) });
                } else if (!String.IsNullOrWhiteSpace(characterRom)) {
                    Guard.Need(File.Exists(characterRom), "Selected character ROM is missing");
                    result.AddRange(new string[] { "--rom", Path.GetFullPath(characterRom) });
                }
            }
            return result;
        }
        internal static List<string> OptionalSetup(string character, string characterRom) {
            Guard.Need(Array.IndexOf(OptionalCharacters, character) >= 0, "Choose one supported optional game");
            if (!String.IsNullOrWhiteSpace(characterRom)) Guard.Need(File.Exists(characterRom), "Selected game ROM is missing");
            return Setup(character, "", characterRom, "", "", false);
        }
        internal static string OptionalRomFilter(string character) {
            Guard.Need(Array.IndexOf(OptionalCharacters, character) >= 0, "Choose one supported optional game");
            if (character == "link" || character == "bomberman") return "N64 ROM|*.z64;*.v64;*.n64|All files|*.*";
            if (character == "banjo") return "N64 ROM or single-ROM ZIP|*.z64;*.v64;*.n64;*.zip|All files|*.*";
            return "Big-endian z64 or single-ROM ZIP|*.z64;*.zip|All files|*.*";
        }
        internal static string OptionalRomFormats(string character) {
            Guard.Need(Array.IndexOf(OptionalCharacters, character) >= 0, "Choose one supported optional game");
            if (character == "link" || character == "bomberman") return "raw .z64, .v64, or .n64 ROM";
            if (character == "banjo") return "raw .z64, .v64, or .n64 ROM, or ZIP containing only that ROM";
            return "big-endian .z64 ROM, or ZIP containing only that ROM";
        }
        internal static List<string> Status() { return new List<string> { "status" }; }
        internal static string DataDirectory(string installBase) { return Path.Combine(Path.GetFullPath(installBase), "data"); }
        internal static string CancelPath(string dataDirectory, Guid operationId) {
            return Path.Combine(Path.GetFullPath(dataDirectory), ".runtime", "cancel-" + operationId.ToString("N"));
        }
        internal static List<string> Play(string mode, string character, string host, int port, string name, bool mute = false) {
            Guard.Need(mode == "single" || mode == "wheel" || mode == "host" || mode == "join", "Unsupported play mode");
            Guard.Need(Array.IndexOf(Characters, character) >= 0, "Unsupported character");
            List<string> result = new List<string> { "play", "--mode", mode, "--character", character };
            if (mute) result.Add("--mute");
            if (mode == "host" || mode == "join") {
                Guard.Need(character == "mario" || character == "octane", "Online supports Mario and Octane only; choose one before Host/Join");
                Guard.Need(port >= 1024 && port <= 65535, "Port must be 1024-65535");
                result.AddRange(new string[] { "--port", port.ToString(CultureInfo.InvariantCulture) });
                if (mode == "host") result.Add("--listen-on-network");
                if (mode == "join") {
                    Guard.Need(!String.IsNullOrEmpty(host) && !host.StartsWith("-") && Regex.IsMatch(host, @"^[A-Za-z0-9.:%_-]{1,253}$"), "Enter a host address without URL or spaces");
                    result.AddRange(new string[] { "--host", host });
                }
                if (!String.IsNullOrEmpty(name)) {
                    Guard.Need(name.Length <= 30, "Player name must be at most 30 characters");
                    foreach (char c in name) Guard.Need(!Char.IsControl(c), "Invalid player name");
                    result.AddRange(new string[] { "--name", name });
                }
            }
            return result;
        }
        internal static ProcessStartInfo StartInfo(string root, string dataDirectory, string cancelFile, IEnumerable<string> args) {
            ProcessStartInfo info = new ProcessStartInfo();
            info.FileName = Guard.Child(root, "python/python.exe");
            StringBuilder command = new StringBuilder("-I -B -u -X utf8 ");
            command.Append(Quote(Guard.Child(root, "codex/windows/seven_launcher.py")));
            foreach (string argument in args) { command.Append(' '); command.Append(Quote(argument)); }
            command.Append(" --data-dir "); command.Append(Quote(Path.GetFullPath(dataDirectory)));
            if (!String.IsNullOrEmpty(cancelFile)) {
                bool setup = false;
                foreach (string argument in args) if (argument == "setup") { setup = true; break; }
                Guard.Need(setup, "Cooperative cancellation is supported for setup only");
                command.Append(" --cancel-file "); command.Append(Quote(Path.GetFullPath(cancelFile)));
            }
            info.Arguments = command.ToString(); info.WorkingDirectory = root;
            info.UseShellExecute = false; info.CreateNoWindow = true; info.WindowStyle = ProcessWindowStyle.Hidden;
            info.RedirectStandardOutput = true; info.RedirectStandardError = true; info.RedirectStandardInput = true;
            info.StandardOutputEncoding = Encoding.UTF8; info.StandardErrorEncoding = Encoding.UTF8;
            string[] allowed = { "PATH", "SYSTEMROOT", "WINDIR", "PATHEXT", "TEMP", "TMP", "USERPROFILE", "HOMEDRIVE", "HOMEPATH", "APPDATA", "LOCALAPPDATA", "PROGRAMDATA", "PROGRAMFILES(X86)", "LANG", "LC_ALL" };
            Dictionary<string, string> keep = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            foreach (string key in allowed) if (Environment.GetEnvironmentVariable(key) != null) keep[key] = Environment.GetEnvironmentVariable(key);
            lock (environmentLock) {
                NormalizeProcessEnvironmentAliases();
                info.EnvironmentVariables.Clear();
                foreach (KeyValuePair<string, string> item in keep) info.EnvironmentVariables[item.Key] = item.Value;
                info.EnvironmentVariables["PYTHONDONTWRITEBYTECODE"] = "1";
                info.EnvironmentVariables["PYTHONUTF8"] = "1";
            }
            return info;
        }
    }

    internal sealed class ShareAddress {
        public string Address { get; set; }
        public string Kind { get; set; }
        public override string ToString() { return Address + "  (" + Kind + ")"; }
    }

    internal sealed class AddressReport {
        public List<ShareAddress> Addresses { get; set; }
        public string Status { get; set; }
        public bool TailscaleConnected { get; set; }
    }

    internal static class NetworkAddresses {
        // Local inventory only. Never runs up/login/set, installs Tailscale,
        // enumerates peers, probes a remote device or changes firewall rules.
        internal static bool IsTailscaleIPv4(string value) {
            IPAddress ip;
            if (!IPAddress.TryParse(value, out ip) || ip.AddressFamily != AddressFamily.InterNetwork || ip.ToString() != value) return false;
            byte[] bytes = ip.GetAddressBytes();
            return bytes[0] == 100 && bytes[1] >= 64 && bytes[1] <= 127 && value != "100.100.100.100";
        }
        internal static bool IsPrivateLanIPv4(string value) {
            IPAddress ip;
            if (!IPAddress.TryParse(value, out ip) || ip.AddressFamily != AddressFamily.InterNetwork || ip.ToString() != value) return false;
            byte[] bytes = ip.GetAddressBytes();
            return bytes[0] == 10 || (bytes[0] == 172 && bytes[1] >= 16 && bytes[1] <= 31) || (bytes[0] == 192 && bytes[1] == 168);
        }
        internal static string ConnectedIPv4(string text, out string state) {
            state = "unavailable";
            try {
                if (String.IsNullOrEmpty(text) || text.Length > 1024 * 1024) return null;
                JavaScriptSerializer json = new JavaScriptSerializer { MaxJsonLength = 1024 * 1024, RecursionLimit = 24 };
                Dictionary<string, object> data = json.DeserializeObject(text) as Dictionary<string, object>;
                object backend, selfValue, online, addresses;
                if (data == null || !data.TryGetValue("BackendState", out backend)) return null;
                state = backend as string ?? "unavailable";
                Dictionary<string, object> self = data.TryGetValue("Self", out selfValue) ? selfValue as Dictionary<string, object> : null;
                if (state != "Running" || self == null || !self.TryGetValue("Online", out online) || !(online is bool) || !(bool)online) return null;
                object[] ips = data.TryGetValue("TailscaleIPs", out addresses) ? addresses as object[] : null;
                if (ips != null) foreach (object item in ips) if (item is string && IsTailscaleIPv4((string)item)) return (string)item;
            } catch (ArgumentException) { } catch (InvalidOperationException) { }
            return null;
        }
        internal static AddressReport Compose(IEnumerable<string> tailscaleAdapters, IEnumerable<string> lanAdapters, string cliJson) {
            string state;
            string connected = ConnectedIPv4(cliJson, out state);
            AddressReport result = new AddressReport { Addresses = new List<ShareAddress>(), TailscaleConnected = connected != null };
            HashSet<string> seen = new HashSet<string>(StringComparer.Ordinal);
            if (connected != null) { result.Addresses.Add(new ShareAddress { Address = connected, Kind = "Tailscale" }); seen.Add(connected); }
            // A stopped/logged-out daemon can leave an adapter IP behind. Do not
            // recommend it when the authoritative local status is available.
            if (connected == null && state == "unavailable") foreach (string address in tailscaleAdapters)
                if (IsTailscaleIPv4(address) && seen.Add(address)) result.Addresses.Add(new ShareAddress { Address = address, Kind = "Tailscale adapter; connection unconfirmed" });
            int tailscaleCount = result.Addresses.Count;
            foreach (string address in lanAdapters) if (IsPrivateLanIPv4(address) && seen.Add(address))
                result.Addresses.Add(new ShareAddress { Address = address, Kind = "LAN / private adapter" });
            result.Status = connected != null ? "Tailscale is connected on this PC. Your friend must also be connected and allowed to reach this PC." :
                tailscaleCount > 0 ? "Tailscale adapter detected; connection could not be confirmed. Check Tailscale, then Refresh addresses." :
                "No connected Tailscale IPv4 detected. Connect Tailscale separately, then Refresh addresses, or use a reachable LAN address.";
            return result;
        }
        private static string ReadStatus() {
            foreach (Environment.SpecialFolder folder in new Environment.SpecialFolder[] { Environment.SpecialFolder.ProgramFiles, Environment.SpecialFolder.ProgramFilesX86 }) {
                string basePath = Environment.GetFolderPath(folder);
                if (String.IsNullOrEmpty(basePath)) continue;
                string executable = Path.Combine(basePath, "Tailscale", "tailscale.exe");
                if (!File.Exists(executable)) continue;
                try {
                    Guard.NoRedirect(executable);
                    ProcessStartInfo info = new ProcessStartInfo(executable, "status --json --peers=false") {
                        UseShellExecute = false, CreateNoWindow = true, WindowStyle = ProcessWindowStyle.Hidden,
                        RedirectStandardOutput = true, RedirectStandardError = true, RedirectStandardInput = true,
                        StandardOutputEncoding = Encoding.UTF8, StandardErrorEncoding = Encoding.UTF8
                    };
                    using (Process process = new Process { StartInfo = info }) {
                        if (!process.Start()) return null;
                        process.StandardInput.Close();
                        Task<string> stdout = process.StandardOutput.ReadToEndAsync();
                        Task<string> stderr = process.StandardError.ReadToEndAsync();
                        if (!process.WaitForExit(3000)) { try { process.Kill(); } catch (InvalidOperationException) { } return null; }
                        if (!Task.WaitAll(new Task[] { stdout, stderr }, 1000) || process.ExitCode != 0) return null;
                        return stdout.Result.Length <= 1024 * 1024 ? stdout.Result : null;
                    }
                } catch (Exception error) {
                    if (!(error is IOException || error is UnauthorizedAccessException || error is System.ComponentModel.Win32Exception || error is InvalidOperationException || error is AggregateException)) throw;
                    return null;
                }
            }
            return null;
        }
        internal static AddressReport Read() {
            List<string> tailscale = new List<string>(), lan = new List<string>();
            try {
                foreach (NetworkInterface adapter in NetworkInterface.GetAllNetworkInterfaces()) {
                    if (adapter.OperationalStatus != OperationalStatus.Up || adapter.NetworkInterfaceType == NetworkInterfaceType.Loopback) continue;
                    bool isTailscale = (adapter.Name + " " + adapter.Description).IndexOf("Tailscale", StringComparison.OrdinalIgnoreCase) >= 0;
                    foreach (UnicastIPAddressInformation address in adapter.GetIPProperties().UnicastAddresses) {
                        string value = address.Address.ToString();
                        if (isTailscale && IsTailscaleIPv4(value)) tailscale.Add(value);
                        else if (IsPrivateLanIPv4(value)) lan.Add(value);
                    }
                }
            } catch (NetworkInformationException) { } catch (UnauthorizedAccessException) { }
            tailscale.Sort(StringComparer.Ordinal); lan.Sort(StringComparer.Ordinal);
            return Compose(tailscale, lan, ReadStatus());
        }
    }

    internal sealed partial class LauncherForm : Form {
        private readonly TextBox install = new TextBox(), rom = new TextBox(), game = new TextBox();
        private readonly TextBox host = new TextBox(), player = new TextBox();
        private readonly NumericUpDown port = new NumericUpDown();
        private readonly ComboBox onlineCharacter = new ConceptComboBox(), shareAddresses = new ConceptComboBox();
        private readonly Button copyAddress = new ConceptButton(), refreshAddresses = new ConceptButton(), cancelOperation = new ConceptButton();
        private readonly Button startHost, joinGame;
        private readonly Label addressStatus = new ConceptLabel(), onlineHeading = new ConceptLabel(), notice = new ConceptLabel();
        private readonly FlowLayoutPanel homePage = NewPage(), setupPage = NewPage(), onlineChoicePage = NewPage(), onlinePage = NewPage();
        private readonly Panel pageHost = new Panel();
        private readonly FlowLayoutPanel hostAddressRow = Row(), joinAddressRow = Row();
        private readonly CheckBox mute = new ConceptCheckBox();
        private bool running, cancellationAvailable;
        private volatile bool cancelRequested;
        private volatile string activeCancelFile;
        private string onlineMode;
        internal LauncherForm(string installBase) {
            Text = "Super Rocket 64"; ClientSize = new Size(900, 700); MinimumSize = new Size(750, 580);
            StartPosition = FormStartPosition.CenterScreen; AutoScaleMode = AutoScaleMode.Font;
            Font = new Font("Segoe UI", 10); BackColor = Color.White;
            install.Text = installBase;
            AddPageText(homePage, "Super Rocket 64", "Jump, boost and fly through the Mushroom Kingdom.");
            AddButton(homePage, "Play Offline", PlayOffline);
            AddButton(homePage, "Online", delegate { ShowPage(onlineChoicePage); });
            AddButton(homePage, "Setup / repair / add characters", ShowSetupPage);
            mute.Text = "Mute this game session"; mute.AutoSize = true; homePage.Controls.Add(mute);

            AddPageText(onlineChoicePage, "Online", "Play over a reachable LAN or an existing Tailscale connection. Configure Tailscale yourself before playing; this launcher does not change network or firewall settings. Choose Host or Join.");
            AddButton(onlineChoicePage, "Host", delegate { SetOnlineMode("host"); });
            AddButton(onlineChoicePage, "Join", delegate { SetOnlineMode("join"); });
            AddButton(onlineChoicePage, "Back", delegate { ShowPage(homePage); });

            onlineHeading.AutoSize = true; onlineHeading.MaximumSize = new Size(820, 0); onlinePage.Controls.Add(onlineHeading);
            onlineCharacter.DropDownStyle = ComboBoxStyle.DropDownList; onlineCharacter.Items.AddRange(new object[] { "Octane", "Mario" }); onlineCharacter.SelectedIndex = 0; onlineCharacter.Width = 145;
            FlowLayoutPanel onlineCharacterRow = Row(); onlineCharacterRow.Controls.Add(new Label { Text = "Play as", AutoSize = true, Padding = new Padding(0, 6, 0, 0) }); onlineCharacterRow.Controls.Add(onlineCharacter); onlinePage.Controls.Add(onlineCharacterRow);
            port.Minimum = 1024; port.Maximum = 65535; port.Value = 7777; port.Width = 90;
            player.Width = 180; player.MaxLength = 30;
            FlowLayoutPanel playerRow = Row(); playerRow.Controls.Add(new Label { Text = "Port", AutoSize = true, Padding = new Padding(0, 6, 0, 0) }); playerRow.Controls.Add(port); playerRow.Controls.Add(new Label { Text = "Your name (optional)", AutoSize = true, Padding = new Padding(8, 6, 0, 0) }); playerRow.Controls.Add(player); onlinePage.Controls.Add(playerRow);
            shareAddresses.DropDownStyle = ComboBoxStyle.DropDownList; shareAddresses.Width = 360;
            hostAddressRow.Controls.Add(new Label { Text = "This PC's LAN / Tailscale addresses", AutoSize = true, Padding = new Padding(0, 6, 0, 0) }); hostAddressRow.Controls.Add(shareAddresses);
            copyAddress.Text = "Copy selected"; copyAddress.AutoSize = true; copyAddress.Enabled = false;
            copyAddress.Click += delegate { try { ShareAddress selected = shareAddresses.SelectedItem as ShareAddress; if (selected != null) { Clipboard.SetText(selected.Address + ":" + port.Value.ToString(CultureInfo.InvariantCulture)); Log("Copied " + selected.Kind + " host address " + selected.Address + ". Share it with the same port shown above."); } } catch (System.Runtime.InteropServices.ExternalException) { Log("Clipboard busy. Select and copy the address manually, or try again."); } };
            refreshAddresses.Text = "Refresh addresses"; refreshAddresses.AutoSize = true; refreshAddresses.Click += delegate { RefreshAddresses(); };
            hostAddressRow.Controls.Add(copyAddress); hostAddressRow.Controls.Add(refreshAddresses); onlinePage.Controls.Add(hostAddressRow);
            addressStatus.Text = "Reading this PC's existing addresses..."; addressStatus.AutoSize = true; addressStatus.MaximumSize = new Size(820, 0); onlinePage.Controls.Add(addressStatus);
            onlinePage.Controls.Add(TextBlock("Tailscale hosts share a 100.x address; LAN hosts share a reachable private address. Hosting listens on the selected port for other PCs. Share a reachable address listed above, never 0.0.0.0 or a loopback address. Every peer needs a matching Super Rocket 64 build and its own Octane setup; only Mario and Octane are available online."));
            host.Text = ""; host.Width = 260; host.MaxLength = 253; host.AccessibleName = "Friend's host address";
            joinAddressRow.Controls.Add(new Label { Text = "Friend's host address", AutoSize = true, Padding = new Padding(0, 6, 0, 0) }); joinAddressRow.Controls.Add(host);
            Button paste = new ConceptButton { Text = "Paste", AutoSize = true }; paste.Click += delegate { try { if (Clipboard.ContainsText()) { string address = Clipboard.GetText().Trim(); int selectedPort = (int)port.Value; ParseEndpoint(ref address, ref selectedPort); host.Text = address; port.Value = selectedPort; } } catch (Exception error) { notice.Text = PlainFailure(error.Message); } };
            joinAddressRow.Controls.Add(paste); onlinePage.Controls.Add(joinAddressRow);
            startHost = AddButton(onlinePage, "Start host", delegate { PlayOnline("host"); });
            joinGame = AddButton(onlinePage, "Join game", delegate { PlayOnline("join"); });
            AddButton(onlinePage, "Back", delegate { ShowPage(onlineChoicePage); });

            pageHost.Dock = DockStyle.Fill; pageHost.AutoScroll = true;
            foreach (Control page in new Control[] { homePage, setupPage, onlineChoicePage, onlinePage }) pageHost.Controls.Add(page);
            TableLayoutPanel layout = new TableLayoutPanel { Dock = DockStyle.Fill, RowCount = 3, ColumnCount = 1, Padding = new Padding(20) };
            layout.RowStyles.Add(new RowStyle(SizeType.Percent, 100)); layout.RowStyles.Add(new RowStyle(SizeType.AutoSize)); layout.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            FlowLayoutPanel operations = Row(); cancelOperation.Text = "Cancel setup"; cancelOperation.AutoSize = true; cancelOperation.Visible = false; cancelOperation.Click += delegate { CancelOperation(); }; operations.Controls.Add(cancelOperation);
            notice.AutoSize = true; notice.MaximumSize = new Size(810, 110);
            layout.Controls.Add(pageHost, 0, 0); layout.Controls.Add(operations, 0, 1); layout.Controls.Add(notice, 0, 2);
            Controls.Add(layout);
            InitializeUpdates(); InitializeWizard(); InitializePresentation();
            ShowPage(locationPage);
            Shown += delegate { StartupUpdates(); };
            FormClosing += delegate(object sender, FormClosingEventArgs e) { if (running) { e.Cancel = true; MessageBox.Show(this, updateCancellation != null ? "Use Cancel update if available, then wait for update verification or restart to finish." : cancellationAvailable ? "Use Cancel setup, then wait for the helper to finish cleaning its private stage." : "Wait for the current operation to finish. The launcher does not cancel gameplay.", Text); } };
        }
        private static FlowLayoutPanel NewPage() { return new FlowLayoutPanel { FlowDirection = FlowDirection.TopDown, WrapContents = false, AutoSize = true, Dock = DockStyle.Top, Padding = new Padding(4) }; }
        private static FlowLayoutPanel Row() { return new FlowLayoutPanel { AutoSize = true, WrapContents = true, Margin = new Padding(0, 3, 0, 3) }; }
        private static Label TextBlock(string text) { return new ConceptLabel { Text = text, AutoSize = true, MaximumSize = new Size(820, 0), Padding = new Padding(0, 5, 0, 5) }; }
        private static void AddPageText(FlowLayoutPanel page, string heading, string description) {
            page.Controls.Add(new Label { Text = heading, AutoSize = true, Font = new Font(SystemFonts.DefaultFont, FontStyle.Bold), Padding = new Padding(0, 6, 0, 0) });
            page.Controls.Add(TextBlock(description));
        }
        private void ShowPage(FlowLayoutPanel page) {
            foreach (Control item in pageHost.Controls) item.Visible = Object.ReferenceEquals(item, page);
            page.BringToFront();
            SelectConcept(page);
            if(page==setupPage)QueueSourceValidation();
            else LeaveSourceValidation();
        }
        private void AddPath(FlowLayoutPanel page, string title, TextBox input, bool file) { AddPath(page, title, input, file, null); }
        private void AddPath(FlowLayoutPanel page, string title, TextBox input, bool file, Func<string> fileFilter) {
            FlowLayoutPanel row = Row(); input.Width = 410;
            row.Controls.Add(new Label { Text = title, AutoSize = true, Padding = new Padding(0, 6, 0, 0) }); row.Controls.Add(input);
            Button browse = new ConceptButton { Text = "Browse...", AutoSize = true };
            browse.Click += delegate {
                if (file) { using (OpenFileDialog dialog = new OpenFileDialog { Title = title, Filter = fileFilter == null ? "N64 ROM or single-ROM ZIP|*.z64;*.v64;*.n64;*.zip|All files|*.*" : fileFilter(), CheckFileExists = true }) if (dialog.ShowDialog(this) == DialogResult.OK) input.Text = dialog.FileName; }
                else { using (FolderBrowserDialog dialog = new FolderBrowserDialog { Description = title, ShowNewFolderButton = true }) if (dialog.ShowDialog(this) == DialogResult.OK) input.Text = dialog.SelectedPath; }
            };
            row.Controls.Add(browse); page.Controls.Add(row);
        }
        private Button AddButton(FlowLayoutPanel page, string label, Action action) {
            Button button = new ConceptButton { Text = label, AutoSize = true, MinimumSize = new Size(210, 32) };
            button.Click += delegate { try { action(); } catch (Exception error) { notice.Text = PlainFailure(error.Message); } };
            page.Controls.Add(button);
            return button;
        }
        private string SelectedOnlineCharacter() { return onlineCharacter.SelectedIndex == 1 ? "mario" : "octane"; }
        private void ShowSetupPage() { ShowPage(locationPage); }
        private void SetOnlineMode(string mode) {
            onlineMode = mode; onlineHeading.Text = mode == "host" ? "Host a game" : "Join a game";
            startHost.Visible = mode == "host"; joinGame.Visible = mode == "join";
            hostAddressRow.Visible = mode == "host"; addressStatus.Visible = mode == "host"; joinAddressRow.Visible = mode == "join";
            ShowPage(onlinePage); if (mode == "host") RefreshAddresses();
        }
        private void PlayOnline(string mode) {
            Guard.Need(onlineMode == mode, "Go back and choose Host or Join first");
            string address = host.Text.Trim(); int chosenPort = (int)port.Value;
            if (mode == "join") { ParseEndpoint(ref address, ref chosenPort); host.Text = address; port.Value = chosenPort; }
            BeginOperation(Commands.Play(mode, SelectedOnlineCharacter(), address, chosenPort, player.Text, mute.Checked), false, null);
        }
        private void RefreshAddresses() {
            if (!refreshAddresses.Enabled) return;
            refreshAddresses.Enabled = false; copyAddress.Enabled = false; addressStatus.Text = "Reading this PC's existing addresses...";
            Task.Factory.StartNew(delegate {
                AddressReport report;
                try { report = NetworkAddresses.Read(); }
                catch (Exception) { report = new AddressReport { Addresses = new List<ShareAddress>(), Status = "Address detection unavailable. Copy your host address from Tailscale or enter a known LAN address manually." }; }
                if (IsDisposed || Disposing) return;
                try { BeginInvoke(new Action(delegate { shareAddresses.Items.Clear(); foreach (ShareAddress address in report.Addresses) shareAddresses.Items.Add(address); if (shareAddresses.Items.Count > 0) shareAddresses.SelectedIndex = 0; copyAddress.Enabled = shareAddresses.Items.Count > 0; refreshAddresses.Enabled = true; addressStatus.Text = report.Status; })); } catch (InvalidOperationException) { }
            });
        }
        private void Log(string value) {
            if (IsDisposed || Disposing) return;
            if (InvokeRequired) { try { BeginInvoke(new Action<string>(Log), value); } catch (InvalidOperationException) { } return; }
            // Worker output is not a UI log. Only known stages reach the screen.
            string stage = FriendlyStage(value);
            if (stage != null) progressText.Text = stage;
        }
        private void BeginOperation(List<string> args, bool allowCancellation, Action completed) {
            Guard.Need(!running, "Finish the current operation first."); string basePath = install.Text.Trim();
            Installer.Destination(basePath, PayloadInfo.ZipSha256); running = true; cancelRequested = false; activeCancelFile = null;
            cancellationAvailable = allowCancellation && args.Count > 0 && args[0] == "setup";
            cancelOperation.Text = "Cancel setup"; pageHost.Enabled = false;
            cancelOperation.Visible = cancellationAvailable; cancelOperation.Enabled = cancellationAvailable;
            progressText.Text = args[0] == "play" ? "Opening your game..." : "Checking your installation...";
            progress.Style = ProgressBarStyle.Marquee; ShowPage(progressPage); notice.Text = "";
            Task.Factory.StartNew(delegate {
                bool succeeded = false; string cancelFile = null, errorText = null;
                Dictionary<string, object> report = null;
                try {
                    using (OperationLease lease = OperationLease.Acquire(basePath)) {
                        string root = Installer.Embedded(basePath, Log);
                        if (cancelRequested) throw new OperationCanceledException();
                        string dataDirectory = Commands.DataDirectory(basePath);
                        if (cancellationAvailable) {
                            Guard.NoRedirect(dataDirectory); Directory.CreateDirectory(dataDirectory); Guard.NoRedirect(dataDirectory);
                            string runtime = Path.Combine(dataDirectory, ".runtime"); Guard.NoRedirect(runtime); Directory.CreateDirectory(runtime); Guard.NoRedirect(runtime);
                            cancelFile = Commands.CancelPath(dataDirectory, Guid.NewGuid()); activeCancelFile = cancelFile;
                            if (cancelRequested) WriteCancelMarker(cancelFile);
                        }
                        StringBuilder stdout = new StringBuilder(), stderr = new StringBuilder();
                        using (Process process = new Process { StartInfo = Commands.StartInfo(root, dataDirectory, cancelFile, args) }) {
                            process.OutputDataReceived += delegate(object sender, DataReceivedEventArgs e) { if (e.Data != null) { if (stdout.Length < 1048576) stdout.AppendLine(e.Data); Log(e.Data); } };
                            process.ErrorDataReceived += delegate(object sender, DataReceivedEventArgs e) { if (e.Data != null && stderr.Length < 65536) stderr.AppendLine(e.Data); };
                            Guard.Need(process.Start(), "Could not start setup. Repair the program files and retry."); process.StandardInput.Close();
                            if (cancelRequested && cancelFile != null) WriteCancelMarker(cancelFile);
                            process.BeginOutputReadLine(); process.BeginErrorReadLine(); process.WaitForExit();
                            if (process.ExitCode == 130) throw new OperationCanceledException();
                            Guard.Need(process.ExitCode == 0, PlainFailure(stderr.ToString()));
                            if (args[0] == "wizard-status" || args[0] == "preflight") {
                                report = new JavaScriptSerializer().DeserializeObject(stdout.ToString()) as Dictionary<string, object>;
                                Guard.Need(report != null, "Could not read setup status. Repair the program files and retry.");
                                object valid; if (report.TryGetValue("valid", out valid)) Guard.Need((bool)valid, (string)report["message"]);
                            }
                            succeeded = true;
                        }
                    }
                } catch (OperationCanceledException) { errorText = "Setup paused. Completed characters, saves and controls are safe. Resume to reuse verified work."; }
                catch (Exception error) { errorText = PlainFailure(error.Message); }
                finally {
                    activeCancelFile = null;
                    if (cancelFile != null) try { Guard.NoRedirect(cancelFile); if (File.Exists(cancelFile)) File.Delete(cancelFile); } catch (IOException) { } catch (UnauthorizedAccessException) { }
                    if (!IsDisposed && !Disposing) try { BeginInvoke(new Action(delegate {
                        running = false; cancellationAvailable = false; pageHost.Enabled = true; cancelOperation.Enabled = false; cancelOperation.Visible = false;
                        if (succeeded) { if (report != null) lastReport = report; try { if (completed != null) completed(); else ShowPage(homePage); } catch (Exception problem) { ShowFailure(PlainFailure(problem.Message), delegate { BeginOperation(args, allowCancellation, completed); }, null); } }
                        else ShowFailure(errorText, delegate { BeginOperation(args, allowCancellation, completed); }, args[0] == "setup" && args.Contains("--character") && args[args.IndexOf("--character") + 1] != "octane" ? completed : null, args[0] == "play" && args.Contains("--mode") && (args[args.IndexOf("--mode") + 1] == "host" || args[args.IndexOf("--mode") + 1] == "join") ? onlinePage : setupPage);
                    })); } catch (InvalidOperationException) { }
                }
            });
        }
        private static void WriteCancelMarker(string path) {
            Guard.NoRedirect(Path.GetDirectoryName(path));
            try { using (FileStream stream = new FileStream(path, FileMode.CreateNew, FileAccess.Write, FileShare.Read)) { byte[] marker = Encoding.ASCII.GetBytes("cancel\n"); stream.Write(marker, 0, marker.Length); } }
            catch (IOException) { if (!File.Exists(path)) throw; }
        }
        private void CancelOperation() {
            if (updateCancellation != null) { updateCancellation.Cancel(); cancelOperation.Enabled = false; Log("Cancel requested; waiting for the update operation to stop."); return; }
            if (!running || !cancellationAvailable) return;
            cancelRequested = true; cancelOperation.Enabled = false;
            string marker = activeCancelFile;
            try {
                if (marker != null) { WriteCancelMarker(marker); Log("Cancel requested. Waiting for setup to stop its own converter and clean its private stage."); }
                else progressText.Text = "Cancel requested. Finishing the current verification safely...";
            } catch (Exception error) { cancelOperation.Enabled = true; Log("Could not create setup cancel marker: " + error.Message); }
        }
    }


    internal static class Program {
        [STAThread] internal static int Main(string[] args) {
            bool headless = Array.IndexOf(args, "--verify-only") >= 0 || Array.IndexOf(args, "--list-addresses") >= 0 || Array.IndexOf(args, "--update-probe") >= 0 || Array.IndexOf(args, "--check-updates") >= 0;
            try {
                Options options = Options.Parse(args);
                if (options.CheckUpdates) {
                    ReleaseCheck check = ReleaseUpdates.Check(new OfficialUpdateTransport(), UpdateBuild.Version, System.Threading.CancellationToken.None);
                    Console.WriteLine(new JavaScriptSerializer().Serialize(new { current_version = UpdateBuild.Version, message = check.Message, available_version = check.Available == null ? null : check.Available.Version, game_started = false, download_performed = false }));
                    return 0;
                }
                if (options.ListAddresses) {
                    Console.WriteLine(new JavaScriptSerializer().Serialize(new { local_addresses = NetworkAddresses.Read(), read_only = true, game_started = false, remote_connection_tested = false }));
                    return 0;
                }
                if (options.VerifyOnly || options.UpdateProbe) {
                    string root = Installer.Embedded(options.InstallBase, delegate(string line) { Console.Error.WriteLine(line); });
                    Console.WriteLine(new JavaScriptSerializer().Serialize(new { verified = true, install_root = root, payload_sha256 = PayloadInfo.ZipSha256, game_started = false, helper_started = false, network_used = false, version = UpdateBuild.Version, update_protocol = 1, data_contract = 1 }));
                    return 0;
                }
                if (options.ReadyToken == null && UpdateStore.RouteIfNeeded(options.InstallBase)) return 0;
                Application.EnableVisualStyles(); Application.SetCompatibleTextRenderingDefault(false);
                using (LauncherForm form = new LauncherForm(options.InstallBase)) {
                    form.HandshakeToken = options.ReadyToken;
                    Application.Run(form);
                }
                return 0;
            } catch (Exception error) {
                if (headless) Console.Error.WriteLine("Verification failed: " + error.Message);
                else MessageBox.Show(error.Message, "Super Rocket 64 — stopped", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return 2;
            }
        }
    }
}
