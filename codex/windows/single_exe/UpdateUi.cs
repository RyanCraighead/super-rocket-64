using System;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace SuperRocket64 {
    internal sealed partial class LauncherForm {
        private readonly FlowLayoutPanel settingsPage = NewPage(), updatePage = NewPage();
        private readonly RadioButton automaticUpdates = new RadioButton(), manualUpdates = new RadioButton();
        private readonly CheckBox desktopShortcut = new CheckBox(), menuShortcut = new CheckBox();
        private readonly Label updateMessage = TextBlock(""), settingsMessage = TextBlock("");
        private Button installUpdate, rollbackUpdate, useInstalled;
        private CancellationTokenSource updateCancellation;
        private ReleasePlan availableUpdate;
        internal Func<bool> UpdateGameActive = UpdateStore.GameActive;
        internal IUpdateTransport UpdateTransport = new OfficialUpdateTransport();
        internal Action<string, bool, bool> UpdateShortcuts = LauncherShortcuts.Apply;
        internal string HandshakeToken;
        private void InitializeUpdates() {
            Text += " " + UpdateBuild.DisplayVersion;
            AddButton(homePage, "Settings", ShowUpdateSettings);
            AddPageText(settingsPage, "Updates & shortcuts", "Automatic updates check on startup, download and verify the release, then apply it before play. Manual updates make no startup request. Preview releases are included. You can change your choice here.");
            automaticUpdates.Text = "Keep updated automatically"; automaticUpdates.AutoSize = true;
            manualUpdates.Text = "Manual updates"; manualUpdates.AutoSize = true;
            desktopShortcut.Text = "Desktop shortcut"; desktopShortcut.AutoSize = true;
            menuShortcut.Text = "Start Menu shortcut"; menuShortcut.AutoSize = true;
            settingsPage.Controls.Add(automaticUpdates); settingsPage.Controls.Add(manualUpdates); settingsPage.Controls.Add(desktopShortcut); settingsPage.Controls.Add(menuShortcut); settingsPage.Controls.Add(settingsMessage);
            AddButton(settingsPage, "Save preferences", SaveUpdateSettings);
            AddButton(settingsPage, "Check for updates", delegate { CheckUpdates(false); });
            rollbackUpdate = AddButton(settingsPage, "Roll back launcher", RollbackLauncher);
            AddButton(settingsPage, "Remove launcher shortcuts", RemoveLauncherShortcuts);
            AddButton(settingsPage, "Back", delegate { ShowPage(installationReady ? homePage : locationPage); });
            AddPageText(updatePage, "Launcher update", "Your assets, saves and controller settings stay in the existing data folder. The previous launcher is kept for rollback. A running game is never replaced.");
            updatePage.Controls.Add(updateMessage);
            installUpdate = AddButton(updatePage, "Download update and restart", InstallAvailableUpdate);
            AddButton(updatePage, "Retry update check", delegate { CheckUpdates(false); });
            useInstalled = AddButton(updatePage, "Use installed version", delegate { ShowPage(homePage); });
            AddButton(updatePage, "Settings", ShowUpdateSettings);
            pageHost.Controls.Add(settingsPage); pageHost.Controls.Add(updatePage);
        }
        private string UpdateRoot() { string root = Path.GetFullPath(install.Text.Trim()); Installer.Destination(root, PayloadInfo.ZipSha256); return root; }
        private void ShowUpdateSettings() {
            try {
                string root = UpdateRoot(); UpdatePreferences value = UpdatePreferences.Load(root);
                automaticUpdates.Checked = value.ModeChosen && value.AutomaticApply; manualUpdates.Checked = value.ModeChosen && !value.AutomaticApply;
                desktopShortcut.Checked = value.DesktopShortcut; menuShortcut.Checked = value.StartMenuShortcut;
                rollbackUpdate.Enabled = UpdateStore.Load(root).previous != null;
                settingsMessage.Text = value.ModeChosen ? "Version " + UpdateBuild.DisplayVersion + ". Save to apply your choice." : "Choose how to update. Earlier permission to check for updates does not authorize automatic installation. Your old check-only preference stays in effect until you choose.";
                ShowPage(settingsPage);
            } catch (Exception error) { notice.Text = PlainFailure(error.Message); }
        }
        private void SavePreferences(bool automatic, bool desktop, bool menu) {
            string root = UpdateRoot();
            using (OperationLease lease = OperationLease.Acquire(root)) {
                UpdateStore.EnsureCurrent(root); LauncherShortcuts.EnsureStable(root); UpdateShortcuts(root, desktop, menu);
                UpdatePreferences value = UpdatePreferences.Load(root);
                value.Configured = value.ModeChosen = true; value.AutomaticChecks = value.AutomaticApply = automatic;
                value.DesktopShortcut = desktop; value.StartMenuShortcut = menu; value.Save(root);
            }
        }
        private void SaveUpdateSettings() {
            Guard.Need(automaticUpdates.Checked || manualUpdates.Checked, "Choose automatic or manual updates first.");
            SavePreferences(automaticUpdates.Checked, desktopShortcut.Checked, menuShortcut.Checked);
            notice.Text = "Preferences saved."; ShowPage(installationReady ? homePage : locationPage);
        }
        private void RemoveLauncherShortcuts() {
            string root = UpdateRoot(); using (OperationLease lease = OperationLease.Acquire(root)) {
                UpdateShortcuts(root, false, false); UpdatePreferences value = UpdatePreferences.Load(root); value.DesktopShortcut = value.StartMenuShortcut = false; value.Save(root);
            }
            desktopShortcut.Checked = menuShortcut.Checked = false; notice.Text = "Shortcuts removed. Your installation and game data are kept.";
        }
        private void StartupUpdates() {
            if (HandshakeToken != null) { WaitForUpdateCommit(); return; }
            if (!Directory.Exists(Commands.DataDirectory(UpdateRoot()))) { ShowPage(locationPage); return; }
            InspectInstallation(true);
        }
        private void ContinueStartupUpdates() {
            try {
                string root = UpdateRoot(); UpdatePreferences value = UpdatePreferences.Load(root);
                using (OperationLease lease = OperationLease.Acquire(root)) { UpdateStore.EnsureCurrent(root); LauncherShortcuts.EnsureStable(root); }
                if (value.AutomaticChecks) CheckUpdates(true);
                else if (!value.ModeChosen) ShowUpdateSettings();
            } catch (Exception error) { ShowUpdateFailure("Could not check updates. " + PlainFailure(error.Message)); }
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
                        File.Delete(path); timer.Stop(); timer.Dispose(); HandshakeToken = null; pageHost.Enabled = true; InspectInstallation(true);
                    } else if (DateTime.UtcNow >= deadline) { timer.Stop(); timer.Dispose(); Close(); }
                } catch (Exception) { timer.Stop(); timer.Dispose(); Close(); }
            }; timer.Start();
        }
        private void ShowUpdateFailure(string text) {
            updateMessage.Text = text + " Your installed version and data are retained. Retry when online, or use the installed version if it is ready.";
            installUpdate.Enabled = availableUpdate != null; useInstalled.Visible = installationReady; ShowPage(updatePage);
        }
        private void RunUpdate(Action<CancellationToken> work, Action completed, bool cancellable) {
            if (running) { notice.Text = "Finish the current operation first."; return; }
            running = true; cancellationAvailable = false; pageHost.Enabled = false; notice.Text = "";
            progressText.Text = "Checking and verifying the update..."; progress.Style = ProgressBarStyle.Marquee; ShowPage(progressPage);
            updateCancellation = new CancellationTokenSource(); CancellationToken token = updateCancellation.Token;
            cancelOperation.Text = "Cancel update"; cancelOperation.Visible = cancellable; cancelOperation.Enabled = cancellable;
            Task.Factory.StartNew(delegate {
                bool success = false; string failure = null;
                try { work(token); success = true; }
                catch (OperationCanceledException) { failure = "Update canceled."; }
                catch (Exception error) { failure = "Update could not finish. " + PlainFailure(error.Message); }
                finally {
                    if (!IsDisposed && !Disposing) try { BeginInvoke(new Action(delegate {
                        updateCancellation.Dispose(); updateCancellation = null; running = false; pageHost.Enabled = true; cancelOperation.Visible = false; cancelOperation.Enabled = false;
                        if (success && completed != null) completed(); else ShowUpdateFailure(failure);
                    })); } catch (InvalidOperationException) { }
                }
            });
        }
        private void CheckUpdates(bool automatic) {
            ReleaseCheck result = null;
            RunUpdate(delegate(CancellationToken token) { result = ReleaseUpdates.Check(UpdateTransport, UpdateBuild.Version, token); }, delegate {
                availableUpdate = result.Available;
                if (availableUpdate != null) {
                    UpdatePreferences prefs = UpdatePreferences.Load(UpdateRoot());
                    if (automatic && prefs.ModeChosen && prefs.AutomaticApply && prefs.PausedVersion != availableUpdate.Version) { InstallAvailableUpdate(); return; }
                    updateMessage.Text = result.Message + " Download: " + ((availableUpdate.Size + 1048575) / 1048576) + " MB." + (prefs.PausedVersion == availableUpdate.Version ? " Automatic retry is paused for this version after a failure or rollback. Retry it manually when ready." : "");
                    installUpdate.Enabled = true; useInstalled.Visible = installationReady; ShowPage(updatePage);
                } else { notice.Text = result.Message; if (!UpdatePreferences.Load(UpdateRoot()).ModeChosen) ShowUpdateSettings(); else ShowPage(installationReady ? homePage : locationPage); }
            }, true);
        }
        private void InstallAvailableUpdate() {
            if (availableUpdate == null) return;
            string root = UpdateRoot(); ReleasePlan plan = availableUpdate;
            RunUpdate(delegate(CancellationToken token) {
                using (OperationLease lease = OperationLease.Acquire(root)) {
                    Guard.Need(!UpdateGameActive(), "Close the running game normally, then retry the update.");
                    UpdateStore.EnsureCurrent(root); LauncherShortcuts.EnsureStable(root);
                    // Persist before starting the child so a crash cannot cause a retry loop.
                    UpdatePreferences prefs = UpdatePreferences.Load(root); prefs.PausedVersion = plan.Version; prefs.Save(root);
                    string executable = ReleaseUpdates.Download(plan, root, UpdateTransport, token);
                    UpdateStore.Probe(executable, root, plan, token); token.ThrowIfCancellationRequested();
                    LauncherVersion next = UpdateStore.Capture(root, executable, plan.Version);
                    Invoke(new Action(delegate { cancelOperation.Enabled = false; })); token.ThrowIfCancellationRequested();
                    UpdateStore.Activate(root, next, UpdateGameActive, UpdateStore.StartReady);
                }
            }, delegate { Close(); }, true);
        }
        private void RollbackLauncher() {
            string root = UpdateRoot(); Guard.Need(UpdateStore.Load(root).previous != null, "No previous launcher is available yet.");
            RunUpdate(delegate(CancellationToken token) {
                using (OperationLease lease = OperationLease.Acquire(root)) {
                    LauncherState state = UpdateStore.Load(root);
                    UpdatePreferences prefs = UpdatePreferences.Load(root); prefs.PausedVersion = state.active.version; prefs.AutomaticApply = prefs.AutomaticChecks = false; prefs.ModeChosen = true; prefs.Save(root);
                    UpdateStore.Activate(root, state.previous, UpdateStore.GameActive, UpdateStore.StartReady);
                }
            }, delegate { Close(); }, false);
        }
    }
}
