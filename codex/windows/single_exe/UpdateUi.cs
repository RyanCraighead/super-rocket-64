using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace SuperRocket64 {
    internal sealed partial class LauncherForm {
        private readonly FlowLayoutPanel settingsPage = NewPage(), updatePage = NewPage();
        private readonly RadioButton automaticUpdates = new ConceptRadioButton(), manualUpdates = new ConceptRadioButton();
        private readonly CheckBox desktopShortcut = new ConceptCheckBox(), menuShortcut = new ConceptCheckBox();
        private readonly Label updateMessage = TextBlock(""), settingsMessage = TextBlock("");
        private Button installUpdate, rollbackUpdate, useInstalled, retryUpdate;
        private readonly Button notifyUpdate=new ConceptButton{Text="Update now"},dismissNotification=new ConceptButton{Text="Later"};
        private readonly Label notificationText=new ConceptLabel();
        private bool notificationVisible,updateError;
        private CancellationTokenSource updateCancellation;
        private ReleasePlan availableUpdate;
        private readonly HashSet<string> announcedUpdates=new HashSet<string>();
        private CancellationTokenSource backgroundUpdateCancellation;
        private bool checkingUpdates,pendingUpdateNotice;
        private readonly System.Windows.Forms.Timer updateNoticeTimer=new System.Windows.Forms.Timer{Interval=1000};
        internal Func<bool> UpdateGameActive = UpdateStore.GameActive;
        internal IUpdateTransport UpdateTransport = new OfficialUpdateTransport();
        internal Action<string, bool, bool> UpdateShortcuts = LauncherShortcuts.Apply;
        internal string HandshakeToken;
        private void InitializeUpdates() {
            Text += " " + UpdateBuild.DisplayVersion;
            AddButton(homePage, "Settings", ShowUpdateSettings);
            AddPageText(settingsPage, "Updates & shortcuts", "Update notifications check at startup. Choose Update now or Later. Manual updates make no startup request. Preview releases are included.");
            automaticUpdates.Text = "Notify me about updates"; automaticUpdates.AutoSize = true;
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
            retryUpdate=AddButton(updatePage, "Retry update check", delegate { CheckUpdates(false); });
            useInstalled = AddButton(updatePage, "Back", DismissUpdate);
            notifyUpdate.Click+=delegate{notificationVisible=false;ShowAvailableUpdate();};dismissNotification.Click+=delegate{DismissUpdate();};
            pageHost.Controls.Add(settingsPage); pageHost.Controls.Add(updatePage);
            updateNoticeTimer.Tick+=delegate{if(pendingUpdateNotice&&!running&&!UpdateGameActive()&&homePage.Visible&&ContainsFocus)PresentUpdate(true);};updateNoticeTimer.Start();
            FormClosed+=delegate{updateNoticeTimer.Stop();updateNoticeTimer.Dispose();if(backgroundUpdateCancellation!=null)backgroundUpdateCancellation.Cancel();};
        }
        private string UpdateRoot() { string root = Path.GetFullPath(install.Text.Trim()); Installer.Destination(root, PayloadInfo.ZipSha256); return root; }
        private void ShowUpdateSettings() {
            try {
                string root = UpdateRoot(); UpdatePreferences value = UpdatePreferences.Load(root);
                automaticUpdates.Checked = value.AutomaticChecks; manualUpdates.Checked = !value.AutomaticChecks;
                desktopShortcut.Checked = value.DesktopShortcut; menuShortcut.Checked = value.StartMenuShortcut;
                rollbackUpdate.Enabled = UpdateStore.Load(root).previous != null;
                settingsMessage.Text = "Version " + UpdateBuild.DisplayVersion + ". Updates install only when you choose Update now.";
                ShowPage(settingsPage);
            } catch (Exception error) { notice.Text = PlainFailure(error.Message); }
        }
        private void SavePreferences(bool automatic, bool desktop, bool menu) {
            string root = UpdateRoot();
            using (OperationLease lease = OperationLease.Acquire(root)) {
                UpdateStore.EnsureCurrent(root); LauncherShortcuts.EnsureStable(root); UpdateShortcuts(root, desktop, menu);
                UpdatePreferences value = UpdatePreferences.Load(root);
                value.Configured = value.ModeChosen = true; value.AutomaticChecks = automatic; value.AutomaticApply = false;
                value.DesktopShortcut = desktop; value.StartMenuShortcut = menu; value.Save(root);
            }
        }
        private void SaveUpdateSettings() {
            Guard.Need(automaticUpdates.Checked || manualUpdates.Checked, "Choose automatic or manual updates first.");
            SavePreferences(automaticUpdates.Checked, desktopShortcut.Checked, menuShortcut.Checked);
            if(!automaticUpdates.Checked){pendingUpdateNotice=false;notificationVisible=false;if(backgroundUpdateCancellation!=null)backgroundUpdateCancellation.Cancel();}
            notice.Text = "Preferences saved."; ShowPage(installationReady ? homePage : locationPage);
        }
        private void RemoveLauncherShortcuts() {
            string root = UpdateRoot(); using (OperationLease lease = OperationLease.Acquire(root)) {
                UpdateShortcuts(root, false, false); UpdatePreferences value = UpdatePreferences.Load(root); value.DesktopShortcut = value.StartMenuShortcut = false; value.Save(root);
            }
            desktopShortcut.Checked = menuShortcut.Checked = false; notice.Text = "Shortcuts removed. Your installation and game data are kept.";
        }
        private void StartupUpdates() {
            try {
                installationReady = false;
                if (HandshakeToken != null) { WaitForUpdateCommit(); return; }
                // A data directory or saved update preference is not proof that
                // setup completed. Only the current helper's verified report is.
                if (!Directory.Exists(Commands.DataDirectory(UpdateRoot()))) { ShowPage(locationPage); return; }
                InspectInstallation(true);
            } catch (Exception error) { EndOperation();ShowFailure(PlainFailure(error.Message), StartupUpdates, null, locationPage); }
        }
        private void ContinueStartupUpdates() {
            try {
                string root = UpdateRoot(); UpdatePreferences value = UpdatePreferences.Load(root);
                using (OperationLease lease = OperationLease.Acquire(root)) { UpdateStore.EnsureCurrent(root); LauncherShortcuts.EnsureStable(root); }
                ShowPage(homePage);
                if (value.AutomaticChecks) CheckUpdates(true);
            } catch (Exception error) { ShowUpdateFailure("Could not check updates. " + PlainFailure(error.Message)); }
        }
        private void WaitForUpdateCommit() {
            running=true;pageHost.Enabled = false;installSteps=false;ShowProgress("Updating launcher", "Completing the verified update...");string root = UpdateRoot(), token = HandshakeToken;
            UpdateStore.SignalReady(root, token);
            var timer = new System.Windows.Forms.Timer { Interval = 100 }; DateTime deadline = DateTime.UtcNow.AddSeconds(35);
            timer.Tick += delegate {
                try {
                    string path = UpdateStore.CommitPath(root, token);
                    if (File.Exists(path)) {
                        var value = UpdateJson.Parse(UpdateJson.ReadFile(path));
                        Guard.Need(UpdateJson.Hash(value, "sha256") == Guard.HashFile(UpdateStore.CurrentExe), "Update restart checksum mismatch");
                        File.Delete(path); timer.Stop(); timer.Dispose(); HandshakeToken = null; EndOperation();if(closeAfterOperation){Close();return;}InspectInstallation(true);
                    } else if (DateTime.UtcNow >= deadline) { timer.Stop(); timer.Dispose(); EndOperation();Close(); }
                } catch (Exception) { timer.Stop(); timer.Dispose(); EndOperation();Close(); }
            }; timer.Start();
        }
        private void ShowUpdateFailure(string text) {
            updateError=true;updateMessage.Text = text + " Your installed version and data are retained.";
            installUpdate.Enabled = availableUpdate != null; useInstalled.Visible = true; ShowPage(updatePage);
        }
        private void RunUpdate(Action<CancellationToken> work, Action completed, bool cancellable) {
            if (running) { notice.Text = "Finish the current operation first."; return; }
            running = true;++operationGeneration;cancellationAvailable = false; pageHost.Enabled = false; notice.Text = "";
            installSteps=false;
            var cancellation=new CancellationTokenSource();updateCancellation = cancellation; CancellationToken token = cancellation.Token;
            cancelOperation.Text = "Cancel update"; cancelOperation.Visible = cancellable; cancelOperation.Enabled = cancellable;
            ShowProgress("Updating launcher", "Checking and verifying the update...");
            Task.Factory.StartNew(delegate {
                bool success = false; string failure = null;
                try { work(token); success = true; }
                catch (OperationCanceledException) { failure = "Update canceled."; }
                catch (Exception error) { failure = "Update could not finish. " + PlainFailure(error.Message); }
                finally {
                    OnUi(delegate {
                        updateCancellation = null;cancellation.Dispose();EndOperation();if(closeAfterOperation){Close();return;}
                        if (success && completed != null) completed(); else ShowUpdateFailure(failure);
                    });
                }
            });
        }
        private void CheckUpdates(bool automatic) {
            if(checkingUpdates)return;
            if(automatic){
                if(!UpdatePreferences.Load(UpdateRoot()).AutomaticChecks)return;
                checkingUpdates=true;var cancellation=new CancellationTokenSource();backgroundUpdateCancellation=cancellation;CancellationToken token=cancellation.Token;
                Task.Factory.StartNew(delegate{
                    ReleaseCheck found=null;try{found=ReleaseUpdates.Check(UpdateTransport,UpdateBuild.Version,token);}catch(Exception){}
                    if(!IsDisposed&&!Disposing)try{BeginInvoke(new Action(delegate{try{checkingUpdates=false;backgroundUpdateCancellation=null;if(token.IsCancellationRequested||found==null)return;availableUpdate=found.Available;if(availableUpdate!=null)PresentUpdate(true);}finally{cancellation.Dispose();}}));}catch(InvalidOperationException){cancellation.Dispose();}else cancellation.Dispose();
                });return;
            }
            ReleaseCheck result = null;
            RunUpdate(delegate(CancellationToken token) { result = ReleaseUpdates.Check(UpdateTransport, UpdateBuild.Version, token); }, delegate {
                availableUpdate = result.Available;
                if (availableUpdate != null) {
                    PresentUpdate(false);
                } else { notice.Text = result.Message; ShowPage(installationReady ? homePage : locationPage); }
            }, true);
        }
        private void PresentUpdate(bool automatic){
            if(availableUpdate==null)return;
            if(automatic&&announcedUpdates.Contains(availableUpdate.Version)){pendingUpdateNotice=false;return;}
            if(automatic&&(running||UpdateGameActive()||!homePage.Visible)){pendingUpdateNotice=true;notice.Text="Update "+availableUpdate.Version+" available. Review it when you return to Play.";return;}
            pendingUpdateNotice=false;announcedUpdates.Add(availableUpdate.Version);
            if(automatic){notificationVisible=true;notice.Text="";notificationText.Text="Super Rocket 64 "+availableUpdate.Version+" is available.";ArrangeConcept();return;}
            ShowAvailableUpdate();
        }
        private void ShowAvailableUpdate(){
            updateError=false;notificationVisible=false;notice.Text="";
            updateMessage.Text="Super Rocket 64 "+availableUpdate.Version+(availableUpdate.Preview?" preview":"")+" is available. Download: "+((availableUpdate.Size+1048575)/1048576)+" MB. The launcher will restart after verification.";
            installUpdate.Enabled=true;useInstalled.Visible=true;ShowPage(updatePage);
        }
        private void DismissUpdate(){if(availableUpdate!=null)announcedUpdates.Add(availableUpdate.Version);pendingUpdateNotice=false;notificationVisible=false;notice.Text="";ShowPage(installationReady?homePage:locationPage);}
        private void InstallAvailableUpdate() {
            if (availableUpdate == null) return;
            string root = UpdateRoot(); ReleasePlan plan = availableUpdate;
            RunUpdate(delegate(CancellationToken token) {
                using (OperationLease lease = OperationLease.Acquire(root)) {
                    Guard.Need(!UpdateGameActive(), "Close the running game normally, then retry the update.");
                    UpdateStore.EnsureCurrent(root); LauncherShortcuts.EnsureStable(root);
                    // Persist before starting the child so a crash cannot cause a retry loop.
                    UpdatePreferences prefs = UpdatePreferences.Load(root); prefs.PausedVersion = plan.Version; prefs.Save(root);
                    int generation=operationGeneration;
                    string executable = ReleaseUpdates.Download(plan, root, UpdateTransport, token,delegate(long count,long total){MeasuredProgress(generation,"Downloading update...",count,total);});
                    MeasuredProgress(generation,"Verifying the downloaded update...",0,0);
                    UpdateStore.Probe(executable, root, plan, token); token.ThrowIfCancellationRequested();
                    LauncherVersion next = UpdateStore.Capture(root, executable, plan.Version);
                    Invoke(new Action(delegate { cancelOperation.Enabled = false; })); token.ThrowIfCancellationRequested();
                    MeasuredProgress(generation,"Restarting the launcher safely...",0,0);UpdateStore.Activate(root, next, UpdateGameActive, UpdateStore.StartReady);
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
