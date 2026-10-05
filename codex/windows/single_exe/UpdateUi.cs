using System;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace SuperRocket64 {
    internal sealed partial class LauncherForm {
        private readonly FlowLayoutPanel settingsPage = NewPage(), updatePage = NewPage();
        private readonly CheckBox automaticUpdates = new CheckBox(), desktopShortcut = new CheckBox(), menuShortcut = new CheckBox();
        private readonly Label updateMessage = new Label(), settingsMessage = new Label();
        private Button installUpdate, rollbackUpdate;
        private CancellationTokenSource updateCancellation;
        private ReleasePlan availableUpdate;
        internal IUpdateTransport UpdateTransport = new OfficialUpdateTransport();
        internal Action<string, bool, bool> UpdateShortcuts = LauncherShortcuts.Apply;
        internal string HandshakeToken;
        private void InitializeUpdates() {
            Text += " " + UpdateBuild.DisplayVersion;
            AddButton(homePage, "Updates & settings", ShowUpdateSettings);
            AddPageText(settingsPage, "Updates & shortcuts", "Choose whether to check GitHub on startup. A new version always asks before downloading and restarting. Published previews are included. You can change these choices at any time.");
            automaticUpdates.Text = "Check for updates automatically on startup (opt in)"; automaticUpdates.AutoSize = true;
            desktopShortcut.Text = "Desktop shortcut"; desktopShortcut.AutoSize = true;
            menuShortcut.Text = "Start Menu shortcut"; menuShortcut.AutoSize = true;
            settingsPage.Controls.Add(automaticUpdates); settingsPage.Controls.Add(desktopShortcut); settingsPage.Controls.Add(menuShortcut);
            settingsMessage.AutoSize = true; settingsMessage.MaximumSize = new System.Drawing.Size(810, 0); settingsPage.Controls.Add(settingsMessage);
            AddButton(settingsPage, "Save preferences", SaveUpdateSettings);
            AddButton(settingsPage, "Check for updates now", delegate { CheckUpdates(false); });
            rollbackUpdate = AddButton(settingsPage, "Roll back launcher", RollbackLauncher);
            AddButton(settingsPage, "Remove launcher shortcuts", RemoveLauncherShortcuts);
            AddButton(settingsPage, "Back", delegate { ShowPage(homePage); });
            AddPageText(updatePage, "Launcher update", "Your assets, saves and controller settings stay in the existing data folder. The previous launcher is kept for rollback. Close the game normally before installing.");
            updateMessage.AutoSize = true; updateMessage.MaximumSize = new System.Drawing.Size(810, 0); updatePage.Controls.Add(updateMessage);
            installUpdate = AddButton(updatePage, "Download update and restart", InstallAvailableUpdate);
            AddButton(updatePage, "Later", delegate { ShowPage(homePage); });
            AddButton(updatePage, "Don't tell me again", DisableAutomaticUpdates);
            pageHost.Controls.Add(settingsPage); pageHost.Controls.Add(updatePage);
        }
        private string UpdateRoot() { string root = Path.GetFullPath(install.Text.Trim()); Installer.Destination(root, PayloadInfo.ZipSha256); return root; }
        private void ShowUpdateSettings() {
            try {
                string root = UpdateRoot(); UpdatePreferences value = UpdatePreferences.Load(root);
                automaticUpdates.Checked = value.AutomaticChecks; desktopShortcut.Checked = value.DesktopShortcut; menuShortcut.Checked = value.StartMenuShortcut;
                rollbackUpdate.Enabled = UpdateStore.Load(root).previous != null;
                settingsMessage.Text = value.Configured ? "Version " + UpdateBuild.DisplayVersion + ". Changes take effect when saved." : "First launch: automatic checks are off. Choose your shortcuts, then save to continue. Assets and saves are never removed by shortcut removal.";
                ShowPage(settingsPage);
            } catch (Exception error) { Log("Settings: " + error.Message); }
        }
        private void SaveUpdateSettings() {
            try {
                string root = UpdateRoot();
                using (OperationLease lease = OperationLease.Acquire(root)) {
                    UpdateStore.EnsureCurrent(root); LauncherShortcuts.EnsureStable(root);
                    UpdateShortcuts(root, desktopShortcut.Checked, menuShortcut.Checked);
                    new UpdatePreferences { Configured = true, AutomaticChecks = automaticUpdates.Checked, DesktopShortcut = desktopShortcut.Checked, StartMenuShortcut = menuShortcut.Checked }.Save(root);
                }
                Log("Preferences saved. Shortcuts use the persistent launcher; your game data is unchanged."); ShowPage(homePage);
                if (automaticUpdates.Checked) CheckUpdates(true);
            } catch (Exception error) { Log("Could not save preferences: " + error.Message); }
        }
        private void RemoveLauncherShortcuts() {
            try {
                string root = UpdateRoot(); using (OperationLease lease = OperationLease.Acquire(root)) {
                    UpdateShortcuts(root, false, false);
                    UpdatePreferences value = UpdatePreferences.Load(root); value.DesktopShortcut = value.StartMenuShortcut = false; value.Save(root);
                }
                desktopShortcut.Checked = menuShortcut.Checked = false; Log("Launcher shortcuts removed. Program files, assets, saves and settings are retained.");
            } catch (Exception error) { Log("Could not remove shortcuts: " + error.Message); }
        }
        private void DisableAutomaticUpdates() {
            try { string root = UpdateRoot(); using (OperationLease lease = OperationLease.Acquire(root)) { UpdatePreferences value = UpdatePreferences.Load(root); value.Configured = true; value.AutomaticChecks = false; value.Save(root); } automaticUpdates.Checked = false; Log("Automatic checks disabled. Manual Check for updates remains available."); ShowPage(homePage); }
            catch (Exception error) { Log("Could not save update preference: " + error.Message); }
        }
        private void StartupUpdates() {
            if (HandshakeToken != null) { WaitForUpdateCommit(); return; }
            try {
                string root = UpdateRoot(); UpdatePreferences value = UpdatePreferences.Load(root);
                if (!value.Configured) { ShowUpdateSettings(); return; }
                using (OperationLease lease = OperationLease.Acquire(root)) { UpdateStore.EnsureCurrent(root); LauncherShortcuts.EnsureStable(root); }
                if (value.AutomaticChecks) CheckUpdates(true);
            }
            catch (Exception error) { Log("Startup check unavailable: " + error.Message + ". You can still play or use Updates & settings."); }
        }
        private void WaitForUpdateCommit() {
            pageHost.Enabled = false; string root = UpdateRoot(), token = HandshakeToken;
            UpdateStore.SignalReady(root, token);
            var timer = new System.Windows.Forms.Timer { Interval = 100 }; DateTime deadline = DateTime.UtcNow.AddSeconds(35);
            timer.Tick += delegate {
                try {
                    string path = UpdateStore.CommitPath(root, token);
                    if (File.Exists(path)) {
                        var value = UpdateJson.Parse(UpdateJson.ReadFile(path));
                        Guard.Need(UpdateJson.Hash(value, "sha256") == Guard.HashFile(UpdateStore.CurrentExe), "Update restart checksum mismatch");
                        File.Delete(path); timer.Stop(); timer.Dispose(); HandshakeToken = null; pageHost.Enabled = true; Log("Launcher restart verified. Your existing data is ready.");
                    } else if (DateTime.UtcNow >= deadline) { timer.Stop(); timer.Dispose(); Close(); }
                } catch (Exception) { timer.Stop(); timer.Dispose(); Close(); }
            };
            timer.Start();
        }
        private void RunUpdate(Action<CancellationToken> work, Action completed, bool cancellable) {
            if (running) { Log("Finish the current operation first."); return; }
            running = true; cancellationAvailable = false; pageHost.Enabled = false;
            updateCancellation = new CancellationTokenSource(); CancellationToken token = updateCancellation.Token;
            cancelOperation.Text = "Cancel update"; cancelOperation.Visible = cancellable; cancelOperation.Enabled = cancellable;
            Task.Factory.StartNew(delegate {
                bool success = false;
                try { work(token); success = true; }
                catch (OperationCanceledException) { Log("Update canceled. The current launcher and your data are unchanged."); }
                catch (Exception error) { Log("Update stopped: " + error.Message + ". Your current launcher and data are retained; retry when ready."); }
                finally {
                    if (!IsDisposed && !Disposing) try { BeginInvoke(new Action(delegate {
                        updateCancellation.Dispose(); updateCancellation = null; running = false; pageHost.Enabled = true; cancelOperation.Visible = false; cancelOperation.Enabled = false;
                        if (success && completed != null) completed();
                    })); } catch (InvalidOperationException) { }
                }
            });
        }
        private void CheckUpdates(bool automatic) {
            ReleaseCheck result = null;
            RunUpdate(delegate(CancellationToken token) { result = ReleaseUpdates.Check(UpdateTransport, UpdateBuild.Version, token); }, delegate {
                Log(result.Message); availableUpdate = result.Available;
                if (availableUpdate != null) { updateMessage.Text = result.Message + " Download size: " + ((availableUpdate.Size + 1048575) / 1048576) + " MB."; installUpdate.Enabled = true; ShowPage(updatePage); }
                else if (!automatic) ShowUpdateSettings();
            }, true);
        }
        private void InstallAvailableUpdate() {
            if (availableUpdate == null) return;
            string root;
            try { root = UpdateRoot(); } catch (Exception error) { Log(error.Message); return; }
            ReleasePlan plan = availableUpdate;
            RunUpdate(delegate(CancellationToken token) {
                using (OperationLease lease = OperationLease.Acquire(root)) {
                    Guard.Need(!UpdateStore.GameActive(), "A game is running. Close it normally first");
                    UpdateStore.EnsureCurrent(root); LauncherShortcuts.EnsureStable(root);
                    Log("Downloading and verifying official release " + plan.Version + "...");
                    string executable = ReleaseUpdates.Download(plan, root, UpdateTransport, token);
                    UpdateStore.Probe(executable, root, plan, token); token.ThrowIfCancellationRequested();
                    LauncherVersion next = UpdateStore.Capture(root, executable, plan.Version);
                    Invoke(new Action(delegate { cancelOperation.Enabled = false; })); token.ThrowIfCancellationRequested();
                    UpdateStore.Activate(root, next, UpdateStore.GameActive, UpdateStore.StartReady);
                }
            }, delegate { Log("Update installed and restarted."); Close(); }, true);
        }
        private void RollbackLauncher() {
            string root;
            try { root = UpdateRoot(); if (UpdateStore.Load(root).previous == null) { Log("No previous updater-capable launcher is available yet."); return; } }
            catch (Exception error) { Log(error.Message); return; }
            RunUpdate(delegate(CancellationToken token) { using (OperationLease lease = OperationLease.Acquire(root)) { LauncherState state = UpdateStore.Load(root); UpdateStore.Activate(root, state.previous, UpdateStore.GameActive, UpdateStore.StartReady); } }, delegate { Close(); }, false);
        }
    }
}
