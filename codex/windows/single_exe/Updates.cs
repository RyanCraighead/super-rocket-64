using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Net;
using System.Reflection;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Web.Script.Serialization;

namespace SuperRocket64 {
    internal static class UpdateJson {
        internal static Dictionary<string, object> Parse(byte[] bytes) {
            Guard.Need(bytes.Length <= 2 * 1024 * 1024, "Update metadata is too large");
            var value = new JavaScriptSerializer { MaxJsonLength = 2 * 1024 * 1024, RecursionLimit = 16 }.DeserializeObject(new UTF8Encoding(false, true).GetString(bytes)) as Dictionary<string, object>;
            Guard.Need(value != null, "Invalid update metadata"); return value;
        }
        internal static string Text(Dictionary<string, object> value, string key) { object item; Guard.Need(value.TryGetValue(key, out item) && item is string, "Missing update field: " + key); return (string)item; }
        internal static long Number(Dictionary<string, object> value, string key) { object item; Guard.Need(value.TryGetValue(key, out item) && (item is int || item is long), "Invalid update number: " + key); return Convert.ToInt64(item, CultureInfo.InvariantCulture); }
        internal static bool Bool(Dictionary<string, object> value, string key) { object item; Guard.Need(value.TryGetValue(key, out item) && item is bool, "Invalid update preference: " + key); return (bool)item; }
        internal static string Hash(Dictionary<string, object> value, string key) { string hash = Text(value, key); Guard.Need(Regex.IsMatch(hash, "^[0-9a-f]{64}$"), "Invalid update hash"); return hash; }
        internal static Version VersionOf(string version) { Guard.Need(Regex.IsMatch(version ?? "", @"^(0|[1-9][0-9]{0,4})\.(0|[1-9][0-9]{0,4})\.(0|[1-9][0-9]{0,4})$"), "Unsupported release version"); return new Version(version); }
        internal static byte[] Bytes(object value) { return new UTF8Encoding(false).GetBytes(new JavaScriptSerializer().Serialize(value)); }
        internal static void Atomic(string path, byte[] bytes) {
            Guard.NoRedirect(path); Directory.CreateDirectory(Path.GetDirectoryName(path)); Guard.NoRedirect(path);
            string stage = path + ".tmp-" + Guid.NewGuid().ToString("N");
            try {
                using (FileStream file = new FileStream(stage, FileMode.CreateNew, FileAccess.Write, FileShare.None)) { file.Write(bytes, 0, bytes.Length); file.Flush(true); }
                Guard.NoRedirect(path);
                if (File.Exists(path)) File.Replace(stage, path, null); else File.Move(stage, path);
            } finally { if (File.Exists(stage)) File.Delete(stage); }
        }
        internal static byte[] ReadFile(string path) { Guard.NoRedirect(path); Guard.Need(new FileInfo(path).Length <= 2 * 1024 * 1024, "Update settings are too large"); return File.ReadAllBytes(path); }
    }
    internal sealed class UpdatePreferences {
        internal bool Configured, AutomaticChecks, DesktopShortcut;
        internal bool StartMenuShortcut = true;
        internal static string PathFor(string root) { return Path.Combine(Path.GetFullPath(root), "launcher-preferences.json"); }
        internal static UpdatePreferences Load(string root) {
            string path = PathFor(root); Guard.NoRedirect(path);
            if (!File.Exists(path)) return new UpdatePreferences();
            var data = UpdateJson.Parse(UpdateJson.ReadFile(path)); Guard.Need(UpdateJson.Number(data, "schema") == 1, "Unsupported launcher settings format");
            return new UpdatePreferences { Configured = UpdateJson.Bool(data, "configured"), AutomaticChecks = UpdateJson.Bool(data, "automatic_checks"), DesktopShortcut = data.ContainsKey("desktop_shortcut") && UpdateJson.Bool(data, "desktop_shortcut"), StartMenuShortcut = !data.ContainsKey("start_menu_shortcut") || UpdateJson.Bool(data, "start_menu_shortcut") };
        }
        internal void Save(string root) { UpdateJson.Atomic(PathFor(root), UpdateJson.Bytes(new { schema = 1, configured = Configured, automatic_checks = AutomaticChecks, desktop_shortcut = DesktopShortcut, start_menu_shortcut = StartMenuShortcut })); }
    }
    internal sealed class OperationLease : IDisposable {
        private readonly Mutex mutex;
        private OperationLease(Mutex value) { mutex = value; }
        internal static OperationLease Acquire(string root) {
            string name; using (MemoryStream bytes = new MemoryStream(Encoding.UTF8.GetBytes(Path.GetFullPath(root).TrimEnd('\\').ToUpperInvariant()))) name = Guard.Hash(bytes);
            Mutex mutex = new Mutex(false, "Local\\SuperRocket64-operation-" + name);
            bool held; try { held = mutex.WaitOne(0); } catch (AbandonedMutexException) { held = true; }
            if (!held) { mutex.Dispose(); throw new IOException("Another launcher is using this installation. Finish its setup/game/update first."); }
            return new OperationLease(mutex);
        }
        public void Dispose() { mutex.ReleaseMutex(); mutex.Dispose(); }
    }
    internal interface IUpdateTransport {
        byte[] Read(Uri uri, int limit, CancellationToken token);
        void Download(Uri uri, string destination, long size, CancellationToken token);
    }
    internal sealed class OfficialUpdateTransport : IUpdateTransport {
        internal static bool Allowed(Uri uri, bool initial) {
            if (uri == null || uri.Scheme != "https" || uri.Port != 443 || uri.UserInfo.Length != 0 || uri.Fragment.Length != 0) return false;
            if (uri.Host == "api.github.com") return uri.AbsolutePath == "/repos/RyanCraighead/super-rocket-64/releases" && uri.Query == "?per_page=20";
            if (uri.Host == "github.com") return Regex.IsMatch(uri.AbsolutePath, @"^/RyanCraighead/super-rocket-64/releases/download/v[0-9]+\.[0-9]+\.[0-9]+/Super-Rocket-64(?:-update\.json|-Windows-x64\.exe)$") && uri.Query.Length == 0;
            return !initial && uri.Host == "release-assets.githubusercontent.com";
        }
        private static void Fetch(Uri initial, long limit, Stream output, CancellationToken token) {
            // The standalone launcher targets .NET Framework; opt out of legacy
            // TLS defaults inside this process before creating the first request.
            // Certificate validation and Windows security settings are unchanged.
            AppContext.SetSwitch("Switch.System.Net.DontEnableSchUseStrongCrypto", false);
            AppContext.SetSwitch("Switch.System.Net.DontEnableSystemDefaultTlsVersions", false);
            ServicePointManager.SecurityProtocol = SecurityProtocolType.Tls12;
            Guard.Need(Allowed(initial, true), "Update URL is not an official project release"); Uri uri = initial;
            for (int redirect = 0; redirect <= 4; redirect++) {
                token.ThrowIfCancellationRequested(); Guard.Need(Allowed(uri, redirect == 0), "Unexpected update download redirect");
                HttpWebRequest request = (HttpWebRequest)WebRequest.Create(uri);
                request.AllowAutoRedirect = false; request.Timeout = 20000; request.ReadWriteTimeout = 20000;
                request.UserAgent = "SuperRocket64-Launcher/" + UpdateBuild.Version; request.Accept = uri.Host == "api.github.com" ? "application/vnd.github+json" : "application/octet-stream";
                request.UseDefaultCredentials = false; request.Credentials = null; request.AutomaticDecompression = DecompressionMethods.None;
                if (uri.Host == "api.github.com") request.Headers["X-GitHub-Api-Version"] = "2026-03-10";
                using (token.Register(request.Abort)) {
                    try {
                        using (HttpWebResponse response = (HttpWebResponse)request.GetResponse()) {
                            int status = (int)response.StatusCode;
                            if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308) {
                                Guard.Need(uri.Host != "api.github.com" && redirect < 4, "Unexpected release API redirect");
                                uri = new Uri(uri, response.Headers["Location"]); continue;
                            }
                            Guard.Need(status == 200 && response.ContentLength <= limit, "Unexpected update response or size");
                            using (Stream input = response.GetResponseStream()) { byte[] buffer = new byte[65536]; long count = 0; int read;
                                while ((read = input.Read(buffer, 0, buffer.Length)) > 0) { token.ThrowIfCancellationRequested(); count += read; Guard.Need(count <= limit, "Update download exceeds its expected size"); output.Write(buffer, 0, read); }
                                Guard.Need(response.ContentLength < 0 || count == response.ContentLength, "Update download was truncated");
                            }
                            return;
                        }
                    } catch (WebException) { token.ThrowIfCancellationRequested(); throw; }
                }
            }
            throw new InvalidDataException("Too many update redirects");
        }
        public byte[] Read(Uri uri, int limit, CancellationToken token) { using (MemoryStream output = new MemoryStream()) { Fetch(uri, limit, output, token); return output.ToArray(); } }
        public void Download(Uri uri, string destination, long size, CancellationToken token) { using (FileStream output = new FileStream(destination, FileMode.CreateNew, FileAccess.Write, FileShare.None)) { Fetch(uri, size, output, token); Guard.Need(output.Length == size, "Update executable length mismatch"); output.Flush(true); } }
    }
    internal sealed class ReleasePlan {
        internal string Version, Sha256, PayloadSha256;
        internal long Size;
        internal Uri Download;
        internal bool Preview;
    }
    internal sealed class ReleaseCheck {
        internal string Message;
        internal ReleasePlan Available;
    }
    internal static class ReleaseUpdates {
        internal const string Latest = "https://api.github.com/repos/RyanCraighead/super-rocket-64/releases?per_page=20";
        internal const long MaxExe = 600L * 1024 * 1024;
        private static Dictionary<string, object> Asset(Dictionary<string, object> release, string name, string version) {
            object entries; Guard.Need(release.TryGetValue("assets", out entries) && entries is object[], "Release assets are missing");
            Dictionary<string, object> found = null;
            foreach (object item in (object[])entries) {
                var asset = item as Dictionary<string, object>; Guard.Need(asset != null, "Invalid release asset");
                if (UpdateJson.Text(asset, "name") != name) continue;
                Guard.Need(found == null, "Duplicate release asset"); found = asset;
            }
            Guard.Need(found != null, "This release does not contain the Windows updater files yet. Your current installation is unchanged.");
            Guard.Need(UpdateJson.Text(found, "state") == "uploaded", "Release asset is not ready");
            string expected = "https://github.com/RyanCraighead/super-rocket-64/releases/download/v" + version + "/" + name;
            Guard.Need(UpdateJson.Text(found, "browser_download_url") == expected, "Release asset does not belong to the official project/tag");
            Guard.Need(Regex.IsMatch(UpdateJson.Text(found, "digest"), "^sha256:[0-9a-f]{64}$"), "Release asset lacks a GitHub SHA-256 digest"); return found;
        }
        internal static ReleaseCheck Check(IUpdateTransport transport, string current, CancellationToken token) {
            byte[] response;
            try { response = transport.Read(new Uri(Latest), 2 * 1024 * 1024, token); }
            catch (WebException error) { var http = error.Response as HttpWebResponse; if (http != null && http.StatusCode == HttpStatusCode.NotFound) { http.Dispose(); return new ReleaseCheck { Message = "No public release is available yet. Your current build is unchanged." }; } throw; }
            Guard.Need(response.Length <= 2 * 1024 * 1024, "Release list is too large");
            object[] releases = new JavaScriptSerializer { MaxJsonLength = 2 * 1024 * 1024, RecursionLimit = 16 }.DeserializeObject(new UTF8Encoding(false, true).GetString(response)) as object[];
            Guard.Need(releases != null && releases.Length <= 20, "Invalid public release list");
            Dictionary<string, object> release = null; Version next = null;
            foreach (object item in releases) {
                var candidate = item as Dictionary<string, object>; Guard.Need(candidate != null, "Invalid release entry");
                if (UpdateJson.Bool(candidate, "draft")) continue;
                string candidateTag = UpdateJson.Text(candidate, "tag_name");
                if (!Regex.IsMatch(candidateTag, @"^v(0|[1-9][0-9]{0,4})\.(0|[1-9][0-9]{0,4})\.(0|[1-9][0-9]{0,4})$")) continue;
                Version candidateVersion = UpdateJson.VersionOf(candidateTag.Substring(1));
                if (next == null || candidateVersion > next) { next = candidateVersion; release = candidate; }
            }
            if (release == null) return new ReleaseCheck { Message = "No compatible public release is available. Your current build is unchanged." };
            string version = UpdateJson.Text(release, "tag_name").Substring(1);
            Version installed = UpdateJson.VersionOf(current);
            if (next <= installed) return new ReleaseCheck { Message = "You are up to date (" + current + ")." };
            var metadataAsset = Asset(release, "Super-Rocket-64-update.json", version);
            long metadataSize = UpdateJson.Number(metadataAsset, "size"); Guard.Need(metadataSize > 0 && metadataSize <= 65536, "Invalid update manifest size");
            byte[] metadata = transport.Read(new Uri(UpdateJson.Text(metadataAsset, "browser_download_url")), (int)metadataSize, token);
            using (MemoryStream stream = new MemoryStream(metadata)) Guard.Need(metadata.LongLength == metadataSize && "sha256:" + Guard.Hash(stream) == UpdateJson.Text(metadataAsset, "digest"), "Update manifest checksum mismatch");
            var manifest = UpdateJson.Parse(metadata);
            Guard.Need(UpdateJson.Number(manifest, "schema") == 1 && UpdateJson.Text(manifest, "edition") == "super-rocket-64" && UpdateJson.Text(manifest, "platform") == "windows-x64", "Wrong updater edition/platform");
            Guard.Need(UpdateJson.Number(manifest, "update_protocol") == 1 && UpdateJson.Number(manifest, "data_contract") == 1, "This release requires a different launcher/data format. Your current installation is unchanged.");
            Guard.Need(UpdateJson.Text(manifest, "version") == version && UpdateJson.Text(manifest, "asset") == "Super-Rocket-64-Windows-x64.exe", "Update manifest version/asset mismatch");
            var executable = Asset(release, "Super-Rocket-64-Windows-x64.exe", version);
            long size = UpdateJson.Number(manifest, "size"); string sha = UpdateJson.Hash(manifest, "sha256");
            Guard.Need(size > 0 && size <= MaxExe && UpdateJson.Number(executable, "size") == size && UpdateJson.Text(executable, "digest") == "sha256:" + sha, "Executable size/checksum metadata mismatch");
            bool preview = UpdateJson.Bool(release, "prerelease");
            return new ReleaseCheck { Message = "Super Rocket 64 " + version + (preview ? " preview" : "") + " is available.", Available = new ReleasePlan { Version = version, Size = size, Sha256 = sha, PayloadSha256 = UpdateJson.Hash(manifest, "payload_sha256"), Download = new Uri(UpdateJson.Text(executable, "browser_download_url")), Preview = preview } };
        }
        internal static string Download(ReleasePlan plan, string root, IUpdateTransport transport, CancellationToken token) {
            UpdateJson.VersionOf(plan.Version);
            Guard.Need(Regex.IsMatch(plan.Sha256 ?? "", "^[0-9a-f]{64}$") && plan.Size > 0 && plan.Size <= MaxExe, "Invalid download plan");
            Guard.Need(plan.Download.AbsoluteUri == "https://github.com/RyanCraighead/super-rocket-64/releases/download/v" + plan.Version + "/Super-Rocket-64-Windows-x64.exe", "Wrong download project/version");
            token.ThrowIfCancellationRequested();
            string directory = Path.Combine(Path.GetFullPath(root), "launcher-versions", "v" + plan.Version + "-" + plan.Sha256.Substring(0, 16));
            Guard.NoRedirect(directory); Directory.CreateDirectory(directory); Guard.NoRedirect(directory);
            string target = Path.Combine(directory, "Super-Rocket-64.exe");
            if (File.Exists(target)) { UpdateStore.Verify(target, plan.Size, plan.Sha256); return target; }
            string partial = target + ".part-" + Guid.NewGuid().ToString("N");
            try { transport.Download(plan.Download, partial, plan.Size, token); token.ThrowIfCancellationRequested(); UpdateStore.Verify(partial, plan.Size, plan.Sha256); Guard.NoRedirect(target); File.Move(partial, target); return target; }
            finally { Guard.NoRedirect(partial); if (File.Exists(partial)) File.Delete(partial); }
        }
    }
    internal sealed class LauncherVersion {
        public string version, path, sha256;
        public long size;
    }
    internal sealed class LauncherState {
        public int schema = 1;
        public LauncherVersion active, previous;
    }
    internal static class UpdateStore {
        internal static string CurrentExe { get { return Assembly.GetExecutingAssembly().Location; } }
        internal static string StatePath(string root) { return Path.Combine(Path.GetFullPath(root), "launcher-state.json"); }
        internal static void Verify(string path, long size, string sha) { Guard.NoRedirect(path); Guard.Need(File.Exists(path) && new FileInfo(path).Length == size && Guard.HashFile(path) == sha, "Launcher file checksum mismatch; the previous version is preserved."); }
        private static LauncherVersion Entry(object item) {
            if (item == null) return null; var value = item as Dictionary<string, object>; Guard.Need(value != null, "Invalid saved launcher version");
            string version = UpdateJson.Text(value, "version"); UpdateJson.VersionOf(version); string sha = UpdateJson.Hash(value, "sha256");
            string path = UpdateJson.Text(value, "path"), expected = "launcher-versions/v" + version + "-" + sha.Substring(0, 16) + "/Super-Rocket-64.exe";
            long size = UpdateJson.Number(value, "size"); Guard.Need(path == expected && size > 0 && size <= ReleaseUpdates.MaxExe, "Invalid saved launcher path/size");
            return new LauncherVersion { version = version, path = path, sha256 = sha, size = size };
        }
        internal static LauncherState Load(string root) {
            string path = StatePath(root); Guard.NoRedirect(path); if (!File.Exists(path)) return new LauncherState();
            var data = UpdateJson.Parse(UpdateJson.ReadFile(path)); Guard.Need(UpdateJson.Number(data, "schema") == 1 && data.ContainsKey("active") && data.ContainsKey("previous"), "Invalid launcher state");
            return new LauncherState { active = Entry(data["active"]), previous = Entry(data["previous"]) };
        }
        internal static void Save(string root, LauncherState state) { UpdateJson.Atomic(StatePath(root), UpdateJson.Bytes(state)); }
        internal static string PathFor(string root, LauncherVersion version) { Guard.Need(version != null, "No saved launcher version"); string path = Guard.Child(Path.GetFullPath(root), version.path); Verify(path, version.size, version.sha256); return path; }
        internal static LauncherVersion Capture(string root, string executable, string version) {
            UpdateJson.VersionOf(version); Guard.NoRedirect(executable); string hash = Guard.HashFile(executable); long size = new FileInfo(executable).Length;
            string relative = "launcher-versions/v" + version + "-" + hash.Substring(0, 16) + "/Super-Rocket-64.exe", path = Guard.Child(Path.GetFullPath(root), relative);
            Directory.CreateDirectory(Path.GetDirectoryName(path)); Guard.NoRedirect(path);
            if (!File.Exists(path)) {
                string stage = path + ".part-" + Guid.NewGuid().ToString("N");
                try { File.Copy(executable, stage, false); Verify(stage, size, hash); File.Move(stage, path); }
                finally { Guard.NoRedirect(stage); if (File.Exists(stage)) File.Delete(stage); }
            }
            Verify(path, size, hash);
            return new LauncherVersion { version = version, path = relative, size = size, sha256 = hash };
        }
        internal static void EnsureCurrent(string root) {
            LauncherState state = Load(root);
            if (state.active != null && UpdateJson.VersionOf(state.active.version) >= UpdateJson.VersionOf(UpdateBuild.Version)) return;
            Guard.Need(!GameActive(), "Close the game normally before registering a new launcher version.");
            state.previous = state.active; state.active = Capture(root, CurrentExe, UpdateBuild.Version); Save(root, state);
        }
        internal static bool GameActive() {
            foreach (Process process in Process.GetProcessesByName("sm64coopdx")) { using (process) { if (!process.HasExited) return true; } }
            return false;
        }
        private static string ReadBounded(StreamReader reader) {
            StringBuilder output = new StringBuilder(); char[] buffer = new char[2048]; int count;
            while ((count = reader.Read(buffer, 0, buffer.Length)) > 0) { Guard.Need(output.Length + count <= 65536, "Launcher verification output exceeded its limit"); output.Append(buffer, 0, count); }
            return output.ToString();
        }
        internal static void Probe(string executable, string root, ReleasePlan expected, CancellationToken token) {
            var info = new ProcessStartInfo(executable, "--update-probe --install-dir " + Commands.Quote(Path.GetFullPath(root))) { UseShellExecute = false, CreateNoWindow = true, RedirectStandardOutput = true, RedirectStandardError = true };
            using (Process process = Process.Start(info)) {
                var stdout = System.Threading.Tasks.Task.Factory.StartNew(delegate { return ReadBounded(process.StandardOutput); });
                var stderr = System.Threading.Tasks.Task.Factory.StartNew(delegate { return ReadBounded(process.StandardError); });
                Stopwatch timer = Stopwatch.StartNew();
                while (!process.WaitForExit(100)) { if (token.IsCancellationRequested || timer.Elapsed > TimeSpan.FromMinutes(3) || stdout.IsFaulted || stderr.IsFaulted) { process.Kill(); process.WaitForExit(); token.ThrowIfCancellationRequested(); throw new IOException("Update package verification stopped or timed out; the previous launcher is unchanged."); } }
                token.ThrowIfCancellationRequested();
                Guard.Need(System.Threading.Tasks.Task.WaitAll(new System.Threading.Tasks.Task[] { stdout, stderr }, 10000), "Launcher verification output did not close");
                Guard.Need(process.ExitCode == 0, "Downloaded launcher could not verify its package; previous version retained.");
                var result = UpdateJson.Parse(Encoding.UTF8.GetBytes(stdout.Result));
                Guard.Need(UpdateJson.Bool(result, "verified") && UpdateJson.Text(result, "version") == expected.Version && UpdateJson.Number(result, "update_protocol") == 1 && UpdateJson.Number(result, "data_contract") == 1 && UpdateJson.Hash(result, "payload_sha256") == expected.PayloadSha256, "Downloaded launcher is incompatible with the update manifest");
            }
        }
        internal static string ReadyPath(string root, string token) { Guard.Need(Regex.IsMatch(token ?? "", "^[a-f0-9]{32}$"), "Invalid ready token"); return Path.Combine(Path.GetFullPath(root), "launcher-ready-" + token + ".json"); }
        internal static void SignalReady(string root, string token) { UpdateJson.Atomic(ReadyPath(root, token), UpdateJson.Bytes(new { sha256 = Guard.HashFile(CurrentExe) })); }
        internal static string CommitPath(string root, string token) { return ReadyPath(root, token) + ".commit"; }
        internal static bool StartReady(string executable, string root, string expectedHash) {
            string token = Guid.NewGuid().ToString("N"), ready = ReadyPath(root, token);
            try {
                var info = new ProcessStartInfo(executable, "--install-dir " + Commands.Quote(Path.GetFullPath(root)) + " --update-ready " + token) { UseShellExecute = false, CreateNoWindow = true };
                using (Process process = Process.Start(info)) {
                    Stopwatch timer = Stopwatch.StartNew();
                    while (timer.Elapsed < TimeSpan.FromSeconds(30)) {
                        if (File.Exists(ready)) {
                            var value = UpdateJson.Parse(UpdateJson.ReadFile(ready));
                            if (UpdateJson.Hash(value, "sha256") == expectedHash) {
                                UpdateJson.Atomic(CommitPath(root, token), UpdateJson.Bytes(new { sha256 = expectedHash }));
                                return true;
                            }
                            // This child cannot start gameplay until the matching commit signal.
                            if (!process.HasExited) { process.Kill(); process.WaitForExit(); }
                            return false;
                        }
                        if (process.WaitForExit(100)) return false;
                    }
                    // The child UI stays disabled until commit; it cannot have launched a game.
                    if (!process.HasExited) { process.Kill(); process.WaitForExit(); }
                    return false;
                }
            } finally { Guard.NoRedirect(ready); if (File.Exists(ready)) File.Delete(ready); }
        }
        internal static void Activate(string root, LauncherVersion next, Func<bool> gameActive, Func<string, string, string, bool> start) {
            Guard.Need(!gameActive(), "A game is running. Close it normally before installing or rolling back an update.");
            string executable = PathFor(root, next); LauncherState before = Load(root);
            Guard.Need(before.active != null, "Current launcher must be registered before updating");
            var after = new LauncherState { active = next, previous = before.active };
            Save(root, after);
            try { Guard.Need(!gameActive(), "A game started while preparing the update. Try again after it closes."); Guard.Need(start(executable, root, next.sha256), "New launcher did not become ready; previous version restored."); }
            catch { Save(root, before); throw; }
        }
        internal static bool RouteIfNeeded(string root) {
            LauncherState state = Load(root); if (state.active == null) return false;
            if (UpdateJson.VersionOf(UpdateBuild.Version) > UpdateJson.VersionOf(state.active.version)) return false;
            if (state.active.sha256 == Guard.HashFile(CurrentExe)) return false;
            try { string path = PathFor(root, state.active); if (StartReady(path, root, state.active.sha256)) return true; }
            catch (IOException) { } catch (UnauthorizedAccessException) { } catch (InvalidDataException) { }
            // Do not change the active pointer from a stale, concurrently opened launcher.
            if (state.previous != null) {
                try { string previous = PathFor(root, state.previous); if (state.previous.sha256 != Guard.HashFile(CurrentExe) && StartReady(previous, root, state.previous.sha256)) return true; }
                catch (IOException) { } catch (UnauthorizedAccessException) { } catch (InvalidDataException) { }
            }
            MessageBoxNotice("The saved update could not start. This preserved launcher will open; use Updates & settings to retry or roll back."); return false;
        }
        private static void MessageBoxNotice(string text) { System.Windows.Forms.MessageBox.Show(text, "Super Rocket 64", System.Windows.Forms.MessageBoxButtons.OK, System.Windows.Forms.MessageBoxIcon.Information); }
    }
}
