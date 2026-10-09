// Headless tests: synthetic payload bytes only; no game/helper/UI/network runs.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Text;
using System.Web.Script.Serialization;

namespace SuperRocket64 {
    internal static class BootstrapTests {
        private static int passed;
        private static string testRoot;
        private static void Check(bool ok, string message) { if (!ok) throw new Exception(message); }
        private static void Test(string name, Action action) { action(); passed++; Console.WriteLine("PASS " + name); }
        private static void Reject(Action action) {
            bool rejected = false;
            try { action(); } catch (InvalidDataException) { rejected = true; } catch (ArgumentException) { rejected = true; }
            Check(rejected, "Expected input rejection");
        }
        private static Dictionary<string, byte[]> Files() {
            return new Dictionary<string, byte[]> {
                { "sm64coopdx.exe", Encoding.ASCII.GetBytes("SYNTHETIC - NOT EXECUTABLE") },
                { "python/python.exe", Encoding.ASCII.GetBytes("SYNTHETIC - NOT EXECUTABLE") },
                { "codex/windows/seven_launcher.py", Encoding.ASCII.GetBytes("# SYNTHETIC fixture, never run\n") },
                { "lang/English.ini", Encoding.ASCII.GetBytes("synthetic=true\n") }
            };
        }
        private static string Sha(byte[] bytes) { using (MemoryStream stream = new MemoryStream(bytes)) return Guard.Hash(stream); }
        private static byte[] MakeZip(string extraName, bool wrongDigest, bool missing, bool symlink) {
            Dictionary<string, byte[]> files = Files();
            Dictionary<string, object> records = new Dictionary<string, object>();
            foreach (KeyValuePair<string, byte[]> item in files) records[item.Key] = new { size = item.Value.Length, sha256 = wrongDigest && item.Key == "sm64coopdx.exe" ? new string('0', 64) : Sha(item.Value) };
            byte[] manifest = Encoding.UTF8.GetBytes(new JavaScriptSerializer().Serialize(new { files = records }));
            using (MemoryStream output = new MemoryStream()) {
                using (ZipArchive zip = new ZipArchive(output, ZipArchiveMode.Create, true)) {
                    foreach (KeyValuePair<string, byte[]> item in files) {
                        if (missing && item.Key == "lang/English.ini") continue;
                        ZipArchiveEntry entry = zip.CreateEntry(item.Key);
                        if (symlink && item.Key == "lang/English.ini") entry.ExternalAttributes = unchecked((int)0xa0000000);
                        using (Stream stream = entry.Open()) stream.Write(item.Value, 0, item.Value.Length);
                    }
                    using (Stream stream = zip.CreateEntry("PACKAGE-MANIFEST.json").Open()) stream.Write(manifest, 0, manifest.Length);
                    if (extraName != null) using (Stream stream = zip.CreateEntry(extraName).Open()) stream.WriteByte(1);
                }
                return output.ToArray();
            }
        }
        private static string Install(byte[] bytes, string name) {
            using (MemoryStream stream = new MemoryStream(bytes))
                return Installer.Install(stream, Sha(bytes), bytes.Length, Path.Combine(testRoot, name), delegate { });
        }
        private static void Extraction() {
            byte[] good = MakeZip(null, false, false, false);
            Test("valid extraction and exact manifest verification", delegate {
                string root = Install(good, "good"); Check(File.ReadAllText(Path.Combine(root, "sm64coopdx.exe")).StartsWith("SYNTHETIC"), "Wrong extracted file");
                Check(!Directory.Exists(Path.Combine(root, ".runtime")), "Verification created a profile");
            });
            Test("repeat verification preserves private save bytes", delegate {
                string root = Install(good, "good"); string save = Path.Combine(root, ".runtime", "windows-mario", "save"); Directory.CreateDirectory(save);
                File.WriteAllText(Path.Combine(save, "sentinel.txt"), "USER DATA");
                Install(good, "good"); Check(File.ReadAllText(Path.Combine(save, "sentinel.txt")) == "USER DATA", "Private file changed");
            });
            Test("measured extraction cancels before atomic commit and resumes", delegate {
                string folder=Path.Combine(testRoot,"cancel-measured"),destination=Installer.Destination(folder,Sha(good));bool canceled=false;long extracted=0,expected=0;
                using(var token=new System.Threading.CancellationTokenSource())using(var stream=new MemoryStream(good))try{
                    Installer.Install(stream,Sha(good),good.Length,folder,delegate{},delegate(string phase,long count,long total){if(phase.StartsWith("Extracting")){Check(count>0&&count<=total,"Invalid measured extraction");token.Cancel();}},token.Token);
                }catch(OperationCanceledException){canceled=true;}
                Check(canceled&&!Directory.Exists(destination)&&Directory.GetDirectories(folder).Length==0,"Canceled extraction committed or leaked its stage");
                using(var stream=new MemoryStream(good))Installer.Install(stream,Sha(good),good.Length,folder,delegate{},delegate(string phase,long count,long total){if(phase.StartsWith("Extracting")){Check(count>=extracted&&count<=total,"Progress not monotonic");extracted=count;expected=total;}});
                Check(extracted>0&&extracted==expected&&Directory.Exists(destination),"Resumed extraction did not finish measured bytes");
            });
            Test("changed installed binary refuses overwrite", delegate {
                string root = Install(good, "changed"); string file = Path.Combine(root, "sm64coopdx.exe"); File.WriteAllText(file, "CHANGED");
                Reject(delegate { Install(good, "changed"); }); Check(File.ReadAllText(file) == "CHANGED", "Changed file overwritten");
            });
            Test("unknown installed immutable file is rejected", delegate {
                string root = Install(good, "unknown"); File.WriteAllText(Path.Combine(root, "rogue.dll"), "x"); Reject(delegate { Install(good, "unknown"); });
            });
            Test("payload SHA and length are pinned before extraction", delegate {
                using (MemoryStream stream = new MemoryStream(good)) Reject(delegate { Installer.Install(stream, new string('0', 64), good.Length, Path.Combine(testRoot, "hash"), delegate { }); });
                using (MemoryStream stream = new MemoryStream(good)) Reject(delegate { Installer.Install(stream, Sha(good), good.Length + 1, Path.Combine(testRoot, "length"), delegate { }); });
            });
            Test("bad per-file SHA rejected and staging cleaned", delegate {
                byte[] bad = MakeZip(null, true, false, false); Reject(delegate { Install(bad, "filehash"); });
                Check(!Directory.Exists(Installer.Destination(Path.Combine(testRoot, "filehash"), Sha(bad))), "Bad payload installed");
                Check(Directory.GetDirectories(Path.Combine(testRoot, "filehash")).Length == 0, "Owned failed stage leaked");
            });
            Test("missing and unmanifested payload files rejected", delegate {
                Reject(delegate { Install(MakeZip(null, false, true, false), "missing"); });
                Reject(delegate { Install(MakeZip("rogue.dll", false, false, false), "extra"); });
            });
            Test("ZIP traversal absolute ADS device and private paths rejected", delegate {
                foreach (string name in new string[] { "../escape", "/root", "C:/outside", "a:stream", "CON.txt", "a/../b", "x\\y", ".runtime/save", "a./b", "a /b", "rom.z64" })
                    Reject(delegate { Install(MakeZip(name, false, false, false), "unsafe"); });
            });
            Test("case collision and symlink ZIP entry rejected", delegate {
                Reject(delegate { Install(MakeZip("SM64COOPDX.EXE", false, false, false), "case"); });
                Reject(delegate { Install(MakeZip(null, false, false, true), "link"); });
            });
            Test("long Unicode and unversioned destinations cannot bypass path constraints", delegate {
                Reject(delegate { Installer.Destination(Path.Combine(testRoot, new string('x', 101)), Sha(good)); });
                Reject(delegate { Installer.Destination(Path.Combine(testRoot, "\u00e9"), Sha(good)); });
                Reject(delegate { Installer.Destination(@"\\not-contacted\share", Sha(good)); });
                Check(Path.GetFileName(Installer.Destination(testRoot, Sha(good))) == Sha(good).Substring(0, 16), "Version namespace missing");
            });
            Test("native Windows redirect path rejected when present", delegate {
                string junction = Path.Combine(Path.GetPathRoot(Environment.SystemDirectory), "Users", "All Users");
                if (Directory.Exists(junction) && (File.GetAttributes(junction) & FileAttributes.ReparsePoint) != 0) Reject(delegate { Guard.NoRedirect(junction); });
                else Console.WriteLine("NOTE standard Windows junction unavailable; ZIP reparse case is independently covered");
            });
        }
        private static void CommandTests() {
            Test("strict headless command parsing", delegate {
                Options options = Options.Parse(new string[] { "--verify-only", "--install-dir", testRoot }); Check(options.VerifyOnly && options.InstallBase == testRoot, "Headless args lost");
                Options defaults = Options.Parse(new string[0]); Check(Path.GetFileName(defaults.InstallBase) == "SuperRocket64", "Public edition must use its own data/install base");
                foreach (string[] args in new string[][] { new string[] { "--verify-only" }, new string[] { "--verify-only", "--verify-only", "--install-dir", testRoot }, new string[] { "--play" }, new string[] { "--install-dir", "--verify-only" } }) Reject(delegate { Options.Parse(args); });
            });
            Test("safe Windows argument quoting handles slash quote shell punctuation", delegate {
                Check(Commands.Quote("") == "\"\"", "Empty quote");
                Check(Commands.Quote("C:\\two words\\") == "\"C:\\two words\\\\\"", "Trailing slash quote");
                Check(Commands.Quote("x\"y") == "\"x\\\"y\"", "Embedded quote");
                Check(Commands.Quote("a&b;$()") == "\"a&b;$()\"", "Unexpected shell interpretation");
                Reject(delegate { Commands.Quote("x\ny"); });
            });
            Test("Host listens for LAN/Tailscale peers by default and Join addresses are bounded", delegate {
                Check(Commands.Play("host", "octane", null, 7777, "").Contains("--listen-on-network"), "Host must accept other PCs by default");
                Check(Commands.Play("host", "mario", null, 7777, "Host").Contains("--listen-on-network"), "Mario host listener missing");
                Check(Commands.Play("join", "octane", "::1", 7777, "Guest").Contains("::1"), "Loopback join lost");
                foreach (string host in new string[] { "https://host", "a b", "--server", "a&b", "" }) Reject(delegate { Commands.Play("join", "mario", host, 7777, ""); });
                Reject(delegate { Commands.Play("host", "mario", null, 5, ""); });
                foreach (string character in Commands.Characters) if (character != "mario" && character != "octane") Reject(delegate { Commands.Play("host", character, null, 7777, ""); });
            });
            Test("hosting ignores the Join-only address and mute is opt-in", delegate {
                foreach (string address in new string[] { "::1", "", "not a valid join address" }) {
                    List<string> args = Commands.Play("host", "octane", address, 7777, "");
                    Check(!args.Contains("--host") && !args.Contains(address) && args.Contains("--listen-on-network") && !args.Contains("--mute"), "Hosting consumed join address or forced mute");
                }
                Check(Commands.Play("join", "mario", "100.80.1.2", 7777, "", true).Contains("--mute"), "Requested mute lost");
                Check(Commands.Play("join", "mario", "100.80.1.2", 7777, "").Contains("100.80.1.2"), "Tailscale destination lost");
            });
            Test("Offline goes directly to the full wheel with Octane selected", delegate {
                List<string> args = Commands.Play("wheel", "octane", "", 7777, "");
                Check(args[0] == "play" && args.Contains("wheel") && args.Contains("octane") && !args.Contains("--listen-on-network") && !args.Contains("--port"), "Offline wheel/default character changed");
            });
            Test("core setup uses SM64 and Rocket League, with pinned extractor consent from Setup", delegate {
                string rom = Path.Combine(testRoot, "fake.z64"); File.WriteAllText(rom, "synthetic only");
                string rocketLeague = Path.Combine(testRoot, "Rocket League"); Directory.CreateDirectory(Path.Combine(rocketLeague, "TAGame"));
                List<string> denied = Commands.Setup("octane", rom, "", "", rocketLeague, false);
                List<string> allowed = Commands.Setup("octane", rom, "", "", rocketLeague, true);
                Check(allowed.Contains("--sm64") && allowed.Contains("--game") && allowed.Contains(rocketLeague), "Core setup inputs missing");
                Check(!denied.Contains("--download-ueviewer") && allowed.Contains("--download-ueviewer") && !allowed.Contains("--guided"), "Setup consent or no-prompt route changed");
                Reject(delegate { Commands.Setup("octane", rom, "", "", Path.Combine(testRoot,"missing-RL"), true); });
                Reject(delegate { Commands.Setup("octane", Path.Combine(testRoot,"missing.z64"), "", "", rocketLeague, true); });
            });
            Test("optional setup names exactly one validated game ROM and reuses core SM64", delegate {
                string rom = Path.Combine(testRoot, "fake.z64"); if (!File.Exists(rom)) File.WriteAllText(rom, "synthetic only");
                foreach (string character in Commands.OptionalCharacters) {
                    List<string> args = Commands.OptionalSetup(character, rom);
                    Check(args.Contains(character) && args.Contains("--rom") && args.Contains(rom) && !args.Contains("--sm64") && !args.Contains("--game") && !args.Contains("--download-ueviewer"), "Optional setup prompted for unrelated inputs: " + character);
                }
                Reject(delegate { Commands.OptionalSetup("mario", rom); });
                Reject(delegate { Commands.OptionalSetup("octane", rom); });
                Reject(delegate { Commands.OptionalSetup("link", Path.Combine(testRoot,"missing-link.z64")); });
            });
            Test("blank setup sources reach helper validation and reuse for every character", delegate {
                foreach (string character in Commands.Characters) {
                    List<string> args = Commands.Setup(character, "", "", "", "", false);
                    Check(args.Count == 3 && args[0] == "setup" && args[2] == character, "Reuse requested unrelated source input: " + character);
                }
                foreach (string character in Commands.OptionalCharacters) Check(Commands.OptionalSetup(character, "").Count == 3, "Optional reuse was blocked");
            });
            Test("optional ROM pickers match each game's validator formats", delegate {
                Check(Commands.OptionalRomFilter("link").Contains("*.z64;*.v64;*.n64") && !Commands.OptionalRomFilter("link").Contains("*.zip"), "Link must use a raw N64 ROM");
                Check(Commands.OptionalRomFilter("bomberman").Contains("*.z64;*.v64;*.n64") && !Commands.OptionalRomFilter("bomberman").Contains("*.zip"), "Bomberman must use a raw N64 ROM");
                Check(Commands.OptionalRomFilter("banjo").Contains("*.z64;*.v64;*.n64;*.zip"), "Banjo ROM/ZIP formats changed");
                foreach (string character in new string[] { "spiderman", "tony" }) Check(Commands.OptionalRomFilter(character).Contains("*.z64;*.zip") && Commands.OptionalRomFormats(character).Contains("big-endian"), "Big-endian ROM formats changed for " + character);
                Reject(delegate { Commands.OptionalRomFilter("octane"); });
            });
            Test("status is a helper action without setup prompts or gameplay", delegate {
                List<string> args = Commands.Status();
                Check(args.Count == 1 && args[0] == "status", "Status should be one noninteractive action");
            });
            Test("stable data and unique setup cancel paths live beside versioned installs", delegate {
                string installBase = Path.Combine(testRoot, "versioned-install");
                string firstInstall = Path.Combine(installBase, "0123456789abcdef"), nextInstall = Path.Combine(installBase, "fedcba9876543210");
                string data = Commands.DataDirectory(installBase);
                Check(data == Path.Combine(installBase, "data") && !data.StartsWith(firstInstall, StringComparison.OrdinalIgnoreCase) && !data.StartsWith(nextInstall, StringComparison.OrdinalIgnoreCase), "Data path is tied to one package version");
                Guid first = Guid.NewGuid(), second = Guid.NewGuid();
                string firstCancel = Commands.CancelPath(data, first), secondCancel = Commands.CancelPath(data, second);
                Check(firstCancel.StartsWith(Path.Combine(data, ".runtime"), StringComparison.OrdinalIgnoreCase) && firstCancel != secondCancel, "Cancel markers are not unique under stable data");
            });
            Test("helper commands always include stable data path; only setup receives a cancel marker", delegate {
                string data = Path.Combine(testRoot, "data with spaces");
                string cancel = Commands.CancelPath(data, Guid.NewGuid());
                ProcessStartInfo setup = Commands.StartInfo(testRoot, data, cancel, new string[] { "setup", "--character", "octane" });
                Check(setup.Arguments.Contains("--data-dir \"" + data + "\"") && setup.Arguments.Contains("--cancel-file \"" + cancel + "\""), "Setup IPC paths missing or unquoted");
                ProcessStartInfo play = Commands.StartInfo(testRoot, data, null, new string[] { "play", "--name", "Name & quoted \"x\"" });
                Check(play.Arguments.Contains("--data-dir \"" + data + "\"") && !play.Arguments.Contains("--cancel-file"), "Gameplay cancellation or data path contract changed");
                Check(play.FileName == Path.Combine(testRoot, "python", "python.exe"), "Wrong interpreter");
                Check(!play.UseShellExecute && play.CreateNoWindow && play.RedirectStandardOutput && play.RedirectStandardError && play.RedirectStandardInput, "Unsafe process flags");
                Check(play.Arguments.StartsWith("-I -B -u -X utf8 ") && play.EnvironmentVariables.ContainsKey("PYTHONDONTWRITEBYTECODE"), "Isolation/UTF8 flags missing");
                Reject(delegate { Commands.StartInfo(testRoot, data, cancel, new string[] { "play" }); });
            });
            Test("bundled helper cannot inherit unrelated process environment variables", delegate {
                string prior = Environment.GetEnvironmentVariable("SM64_ROCKET_QA_OUTPUT");
                try {
                    Environment.SetEnvironmentVariable("SM64_ROCKET_QA_OUTPUT", "must not leak");
                    ProcessStartInfo info = Commands.StartInfo(testRoot, Path.Combine(testRoot, "data"), null, new string[] { "status" });
                    Check(!info.EnvironmentVariables.ContainsKey("SM64_ROCKET_QA_OUTPUT"), "Unapproved environment variable leaked");
                } finally { Environment.SetEnvironmentVariable("SM64_ROCKET_QA_OUTPUT", prior); }
            });
        }
        private static void AddressTests() {
            Test("only canonical Tailscale and private LAN IPv4 addresses are suggested", delegate {
                Check(NetworkAddresses.IsTailscaleIPv4("100.64.0.1") && NetworkAddresses.IsTailscaleIPv4("100.127.255.254"), "CGNAT bounds rejected");
                foreach (string value in new string[] { "100.63.1.2", "100.128.1.2", "100.100.100.100", "::1", "0.0.0.0", "127.0.0.1", "100.80.1.2:7777", "0100.80.1.2", "100.80.1.2\n" }) Check(!NetworkAddresses.IsTailscaleIPv4(value), "Non-device address suggested: " + value);
                Check(NetworkAddresses.IsPrivateLanIPv4("10.0.0.2") && NetworkAddresses.IsPrivateLanIPv4("172.16.0.2") && NetworkAddresses.IsPrivateLanIPv4("192.168.1.2"), "Private LAN address rejected");
                Check(!NetworkAddresses.IsPrivateLanIPv4("172.32.0.2") && !NetworkAddresses.IsPrivateLanIPv4("8.8.8.8") && !NetworkAddresses.IsPrivateLanIPv4("169.254.1.2"), "Non-LAN address suggested");
            });
            Test("connected local Tailscale address takes priority and peer addresses are ignored", delegate {
                string json = "{\"BackendState\":\"Running\",\"Self\":{\"Online\":true},\"TailscaleIPs\":[\"fd7a::1\",\"100.80.1.2\"],\"Peer\":{\"other\":{\"TailscaleIPs\":[\"100.90.2.3\"]}}}";
                AddressReport result = NetworkAddresses.Compose(new string[] { "100.80.1.2" }, new string[] { "192.168.1.2", "192.168.1.2" }, json);
                Check(result.TailscaleConnected && result.Addresses.Count == 2 && result.Addresses[0].Address == "100.80.1.2" && result.Addresses[1].Kind.Contains("LAN"), "Wrong sharing priority or duplicate");
            });
            Test("stopped logged-out and offline Tailscale do not recommend stale adapter addresses", delegate {
                foreach (string state in new string[] { "Stopped", "NeedsLogin", "NeedsMachineAuth", "Running" }) {
                    string json = "{\"BackendState\":\"" + state + "\",\"Self\":{\"Online\":false},\"TailscaleIPs\":[\"100.80.1.2\"]}";
                    AddressReport result = NetworkAddresses.Compose(new string[] { "100.80.1.2" }, new string[] { "192.168.1.2" }, json);
                    Check(!result.TailscaleConnected && result.Addresses.Count == 1 && result.Addresses[0].Kind.Contains("LAN"), "Stale Tailscale address recommended");
                }
            });
            Test("absent inaccessible or malformed CLI falls back with an honest connection label", delegate {
                foreach (string json in new string[] { null, "", "bad json", "{}", "{\"Self\":{}}" }) {
                    AddressReport result = NetworkAddresses.Compose(new string[] { "100.80.1.2", "100.80.1.2" }, new string[0], json);
                    Check(!result.TailscaleConnected && result.Addresses.Count == 1 && result.Addresses[0].Kind.Contains("unconfirmed"), "Fallback overstated connectivity");
                }
                AddressReport absent = NetworkAddresses.Compose(new string[0], new string[0], null);
                Check(absent.Addresses.Count == 0 && absent.Status.Contains("No connected"), "Missing state is not clear");
            });
            Test("address-only headless mode cannot install or verify a payload", delegate {
                Check(Options.Parse(new string[] { "--list-addresses" }).ListAddresses, "Address report not parsed");
                Reject(delegate { Options.Parse(new string[] { "--list-addresses", "--verify-only", "--install-dir", testRoot }); });
                Reject(delegate { Options.Parse(new string[] { "--list-addresses", "--install-dir", testRoot }); });
                Reject(delegate { Options.Parse(new string[] { "--list-addresses", "--list-addresses" }); });
            });
        }
        private static void FolderTests() {
            foreach (string layout in new string[] { "Epic Games/rocketleague", "SteamLibrary/steamapps/common/rocketleague" }) {
                Test("shared folder setup preserves the selected " + layout + " path", delegate {
                    string game = Path.Combine(testRoot, layout.Replace('/', Path.DirectorySeparatorChar));
                    Directory.CreateDirectory(Path.Combine(game, "TAGame", "CookedPCConsole"));
                    List<string> args = Commands.Setup("octane", "", "", "", game, true);
                    Check(args[args.IndexOf("--game") + 1] == Path.GetFullPath(game), "Selected store folder was changed");
                    Check(args[0] == "setup" && args.Contains("octane") && args.Contains("--download-ueviewer"), "Shared validated extraction route changed");
                    Check(!args.Contains("--assets") && !args.Contains("--rom"), "Folder selection bypassed extraction or requested an unrelated game");
                });
            }
        }
        internal static int Main() {
            testRoot = Path.Combine(Path.GetTempPath(), "n64t-" + Guid.NewGuid().ToString("N").Substring(0, 8)); Directory.CreateDirectory(testRoot);
            try { Extraction(); CommandTests(); AddressTests(); FolderTests(); SourceValidationTests.Run(Path.Combine(testRoot,"source-inputs")); Console.WriteLine("PASS " + passed + " headless checks; no UI/game/helper/network started"); return 0; }
            catch (Exception error) { Console.Error.WriteLine(error); return 1; }
            finally { Guard.NoRedirect(testRoot); Directory.Delete(testRoot, true); }
        }
    }
}
