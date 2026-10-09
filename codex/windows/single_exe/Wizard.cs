using System;
using System.Collections;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Net;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace SuperRocket64 {
    internal sealed partial class LauncherForm {
        private readonly FlowLayoutPanel locationPage = NewPage(), extrasPage = NewPage(), progressPage = NewPage(), readyPage = NewPage(), failurePage = NewPage();
        private readonly Label progressText = TextBlock(""), sourceStatus = TextBlock(""), readyStatus = TextBlock(""), failureText = TextBlock("");
        private readonly ProgressBar progress = new ConceptProgress { Width = 640, Height = 20 };
        private readonly CheckBox[] extraChoices = new CheckBox[5];
        private readonly TextBox[] extraSources = new TextBox[5];
        private readonly FlowLayoutPanel extraInputs = NewPage();
        private readonly RadioButton extrasNo = new ConceptRadioButton { Text = "No, just Mario and Octane", Checked = true, AutoSize = true };
        private readonly RadioButton extrasYes = new ConceptRadioButton { Text = "Yes, choose optional characters", AutoSize = true };
        private readonly RadioButton readyAuto = new ConceptRadioButton { Text = "Notify me about updates", Checked = true, AutoSize = true };
        private readonly RadioButton readyManual = new ConceptRadioButton { Text = "Manual updates", AutoSize = true };
        private readonly CheckBox readyDesktop = new ConceptCheckBox { Text = "Desktop shortcut", AutoSize = true }, readyMenu = new ConceptCheckBox { Text = "Start Menu shortcut", Checked = true, AutoSize = true };
        private Dictionary<string, object> lastReport = new Dictionary<string, object>();
        private readonly Queue<List<string>> setupQueue = new Queue<List<string>>();
        private Action retryAction, skipAction;
        private Button skipOptional, failureBack;
        private FlowLayoutPanel failureReturn;
        private bool installationReady;
        private bool addingCharacters;
        private bool installSteps;
        private string progressSubtitle="Working";
        private readonly HashSet<string> readyCharacters = new HashSet<string>();

        private void InitializeWizard() {
            AddPageText(locationPage, "1 of 5 Â· Choose where to install", "Keep your program, reusable assets, saves and controls together in this folder. Select an existing Super Rocket 64 folder to update or repair it.");
            AddPath(locationPage, "Installation folder", install, false);
            AddPageText(locationPage, "Storage", "Setup checks free space before extraction. Allow 1 GB free for program files and temporary work; optional characters may need more. Completed assets are verified and reused. Your whole Rocket League installation is never copied.");
            AddButton(locationPage, "Next", delegate { InspectInstallation(false); });
            AddButton(locationPage, "Cancel", delegate { if (installationReady) ShowPage(homePage); else Close(); });
            AddPageText(setupPage, "2 of 5 Â· Choose your games", "Select only missing sources. Original SM64 US: .z64, .v64, .n64, or a ZIP containing one ROM. Rocket League: Windows Epic or Steam installation containing TAGame. Body and wheel files must match the supported Steam 25535926 profile; identical Epic files work. Other versions may require a new compatibility profile.");
            setupPage.Controls.Add(sourceStatus);
            AddPath(setupPage, "SM64 US ROM", rom, true);
            AddPath(setupPage, "Rocket League folder", game, false);
            setupPage.Controls.Add(TextBlock("Car sounds are optional and checked separately. Unsupported sounds use the game audio fallback; they do not prevent driving. Extraction tools and Python are handled automatically and verified before use."));
            sourceNext=AddButton(setupPage, "Next", ContinueFromSources);
            AddButton(setupPage, "Back", delegate { ShowPage(locationPage); });
            AddButton(setupPage, "Cancel", CancelWizard);
            AddPageText(extrasPage, "3 of 5 Â· Optional characters", "Very WIP: some progression sections may not work with these characters. Switch back to Mario or Octane using the character wheel if stuck. Extra characters are for Offline play; Online supports Mario and Octane only.");
            extrasPage.Controls.Add(extrasNo); extrasPage.Controls.Add(extrasYes);
            string[] names = { "Link Â· Ocarina of Time (US 1.2, compressed retail)", "Bomberman 64 (USA 1.0)", "Banjo-Kazooie (USA Rev 1)", "Spider-Man (N64 USA 1.0)", "Tony Hawk's Pro Skater (N64 USA Rev 1)" };
            for (int i = 0; i < 5; i++) {
                int index = i; var choice = new ConceptCheckBox { Text = names[i], AutoSize = true }; extraChoices[i] = choice;
                extraSources[i] = new TextBox(); extraInputs.Controls.Add(choice);
                var details = NewPage(); details.Dock = DockStyle.None; details.Visible = false;
                details.Controls.Add(TextBlock(Commands.OptionalRomFormats(Commands.OptionalCharacters[i])));
                AddPath(details, "Original ROM (blank reuses ready assets)", extraSources[i], true, delegate { return Commands.OptionalRomFilter(Commands.OptionalCharacters[index]); });
                extraInputs.Controls.Add(details); choice.CheckedChanged += delegate { details.Visible = choice.Checked; };
            }
            extraInputs.Dock = DockStyle.None; extraInputs.Visible = false; extrasPage.Controls.Add(extraInputs);
            extrasYes.CheckedChanged += delegate { extraInputs.Visible = extrasYes.Checked; };
            extrasPage.Controls.Add(TextBlock("Install uses the location you chose, verifies cached tools, and downloads missing official extraction tools only when needed. Internet is needed for those first-time downloads. Your game sources stay local."));
            AddButton(extrasPage, "Install / resume", delegate { ValidateExtras(0); });
            AddButton(extrasPage, "Back", delegate { if(addingCharacters){addingCharacters=false;ShowPage(homePage);}else ShowPage(setupPage); }); AddButton(extrasPage, "Cancel", CancelWizard);
            AddPageText(progressPage, "4 of 5 Â· Getting ready", "Completed assets stay available if you cancel or need to retry.");
            progressPage.Controls.Add(progressText); progressPage.Controls.Add(progress);
            AddPageText(readyPage, "5 of 5 Â· Ready to play", "Your game sources and saves stay on this PC."); readyPage.Controls.Add(readyStatus);
            readyPage.Controls.Add(readyMenu); readyPage.Controls.Add(readyDesktop);
            readyPage.Controls.Add(TextBlock("Choose how to update (change this any time in Settings):")); readyPage.Controls.Add(readyAuto); readyPage.Controls.Add(readyManual);
            readyPage.Controls.Add(TextBlock("Notifications: check at startup and choose Update now or Later. Manual: no startup checks. Assets, controls and saves are preserved."));
            AddButton(readyPage, "Play Offline", delegate { FinishWizard(true); }); AddButton(readyPage, "Open launcher", delegate { FinishWizard(false); });
            AddPageText(failurePage, "Let's finish setup", ""); failurePage.Controls.Add(failureText);
            AddButton(failurePage, "Retry / resume", delegate { if (retryAction != null) retryAction(); });
            skipOptional = AddButton(failurePage, "Skip this optional character", delegate { if (skipAction != null) skipAction(); });
            failureBack = AddButton(failurePage, "Back / change source", delegate { ShowPage(failureReturn ?? setupPage); });
            AddButton(failurePage, "Repair program files", RepairProgramFiles);
            AddButton(failurePage, "Close", delegate { Close(); });
            foreach (Control page in new Control[] { locationPage, extrasPage, progressPage, readyPage, failurePage }) pageHost.Controls.Add(page);
            InitializeSourceValidation();
        }
        private void CancelWizard() { addingCharacters=false; if (installationReady) ShowPage(homePage); else Close(); }
        private void ShowAddCharacters(){
            if(!installationReady){notice.Text="Finish the base setup first.";ShowSetupPage();return;}
            addingCharacters=true;extrasYes.Checked=true;notice.Text="";ShowPage(extrasPage);
        }
        private void InspectInstallation(bool startup) {
            installationReady = false;
            string target = UpdateRoot(); string parent = target;
            while (!Directory.Exists(parent)) parent = Path.GetDirectoryName(parent);
            Guard.Need(new DriveInfo(Path.GetPathRoot(parent)).AvailableFreeSpace >= 1024L * 1024 * 1024 || Directory.Exists(Installer.Destination(target, PayloadInfo.ZipSha256)), "Free at least 1 GB on this drive, or choose another installation folder.");
            BeginOperation(new List<string> { "wizard-status" }, false, delegate { CompleteInspection(startup); });
        }
        private void CompleteInspection(bool startup) {
                ApplyReport();
                if (startup && installationReady) {
                    ShowPage(homePage); ContinueStartupUpdates();
                }
                else {
                    sourceStatus.Text = "";
                    var componentErrors = lastReport["errors"] as Dictionary<string, object>;
                    for (int i = 0; i < extraChoices.Length; i++) {
                        string character = Commands.OptionalCharacters[i]; string label = extraChoices[i].Text.Split(new string[] { " â€” " }, StringSplitOptions.None)[0];
                        extraChoices[i].Text = label + " â€” " + (readyCharacters.Contains(character) ? "ready, reusable" : componentErrors != null && componentErrors.ContainsKey(character) ? "needs repair" : "not installed");
                    }
                    object detected; if (!readyCharacters.Contains("octane") && String.IsNullOrWhiteSpace(game.Text) && lastReport.TryGetValue("detected", out detected)) foreach (object path in (IEnumerable)detected) { game.Text = (string)path; break; }
                    if (startup) ShowPage(locationPage); else ShowPage(setupPage);
                }
        }
        private void ApplyReport() {
            readyCharacters.Clear(); object list;
            if (lastReport.TryGetValue("ready", out list)) foreach (object character in (IEnumerable)list) readyCharacters.Add((string)character);
            object ready, engine; installationReady = lastReport.TryGetValue("playable", out ready) && ready is bool && (bool)ready
                && lastReport.TryGetValue("engine", out engine) && engine is bool && (bool)engine && readyCharacters.Contains("octane");
        }
        private void ValidateExtras(int index) {
            if(addingCharacters&&index==0){bool chosen=false;foreach(var choice in extraChoices)chosen|=choice.Checked;Guard.Need(chosen,"Choose at least one character.");}
            if (!extrasYes.Checked || index == 5) { PrepareQueue(); return; }
            if (!extraChoices[index].Checked) { ValidateExtras(index + 1); return; }
            if(addingCharacters&&!readyCharacters.Contains(Commands.OptionalCharacters[index])&&String.IsNullOrWhiteSpace(extraSources[index].Text)){notice.Text="Choose the original ROM for "+((ConceptCheckBox)extraChoices[index]).DisplayText+".";extraSources[index].Focus();return;}
            var args = Commands.Setup(Commands.OptionalCharacters[index], addingCharacters ? "" : rom.Text, extraSources[index].Text, "", "", false); args[0] = "preflight";
            BeginOperation(args, false, delegate { ValidateExtras(index + 1); });
        }
        private void BuildSetupQueue() {
            setupQueue.Clear(); if(!addingCharacters)setupQueue.Enqueue(Commands.Setup("octane", rom.Text, "", "", game.Text, true));
            if (extrasYes.Checked) for (int i = 0; i < 5; i++) if (extraChoices[i].Checked) setupQueue.Enqueue(Commands.OptionalSetup(Commands.OptionalCharacters[i], extraSources[i].Text));
        }
        private void PrepareQueue() {
            BuildSetupQueue();
            NextSetup();
        }
        private void NextSetup() {
            if (setupQueue.Count > 0) { BeginOperation(setupQueue.Dequeue(), true, NextSetup); return; }
            BeginOperation(new List<string> { "wizard-status" }, false, delegate {
                ApplyReport(); Guard.Need(installationReady, "Base setup is incomplete. Return to sources and retry.");
                if(addingCharacters){addingCharacters=false;notice.Text="Characters added.";ShowPage(homePage);}else ShowReadyPage();
            });
        }
        private void ShowReadyPage() {
                readyStatus.Text = "Mario + Octane are ready.";
                UpdatePreferences prefs = UpdatePreferences.Load(UpdateRoot());
                readyAuto.Checked = prefs.AutomaticChecks; readyManual.Checked = !prefs.AutomaticChecks;
                readyMenu.Checked = prefs.StartMenuShortcut; readyDesktop.Checked = prefs.DesktopShortcut; ShowPage(readyPage);
        }
        private void FinishWizard(bool play) {
            Guard.Need(readyAuto.Checked || readyManual.Checked, "Choose automatic or manual updates before continuing.");
            SavePreferences(readyAuto.Checked, readyDesktop.Checked, readyMenu.Checked);
            ShowPage(homePage); if (play) PlayOffline();
        }
        private void PlayOffline() { BeginOperation(Commands.Play("wheel", "octane", "", 7777, "", mute.Checked), false, null); }
        private void ShowFailure(string message, Action retry, Action skip) { ShowFailure(message, retry, skip, addingCharacters ? extrasPage : setupPage); }
        private void ShowFailure(string message, Action retry, Action skip, FlowLayoutPanel returnTo) {
            failureReturn = returnTo; failureBack.Text = returnTo == onlinePage ? "Back to connection" : "Back / change source";
            failureText.Text = message; retryAction = retry; skipAction = skip; skipOptional.Visible = skip != null; ShowPage(failurePage);
        }
        private void RepairProgramFiles() {
            if (running) return;
            Guard.Need(!gameSessionActive&&!UpdateGameActive(),"Close the game normally before repairing program files.");
            string root = UpdateRoot(); running = true;installSteps=false;progressSubtitle="Repairing program files";int generation=++operationGeneration;pageHost.Enabled = false;progress.Visible=true;progress.Style=ProgressBarStyle.Marquee;ShowPage(progressPage); progressText.Text = "Verifying replacement program files...";
            Task.Factory.StartNew(delegate {
                string error = null;
                try { using (OperationLease lease = OperationLease.Acquire(root)) Installer.RepairEmbedded(root, Log,UpdateGameActive,delegate(string phase,long count,long total){MeasuredProgress(generation,phase,count,total);}); }
                catch (Exception problem) { error = PlainFailure(problem.Message); }
                OnUi(delegate { ClearBusy();if(closeAfterOperation){Close();return;}if (error != null) ShowFailure(error, RepairProgramFiles, null); else InspectInstallation(false); });
            });
        }
        internal static string PlainFailure(string text) {
            if (String.IsNullOrWhiteSpace(text)) return "This step could not finish. Check the selected sources and available disk space, then retry. Completed assets and saves are safe.";
            if (text.Contains("Unsupported Rocket League package version")) return "This Rocket League update does not match the supported car files. Check the supported-source list or wait for a compatible release. Your game installation was not changed.";
            int stopped = text.LastIndexOf("Stopped: ", StringComparison.Ordinal); if (stopped >= 0) text = text.Substring(stopped + 9);
            if (text.Contains("Traceback") || text.Contains(" at SuperRocket64") || text.Contains("Exception:")) return "This step could not finish. Repair program files, check the selected sources, and retry. Your existing data is preserved.";
            return text.Trim().Length > 650 ? text.Trim().Substring(0, 650) + "... Check the selected source and retry." : text.Trim();
        }
        internal static string FriendlyStage(string line) {
            if (line.IndexOf("cancel", StringComparison.OrdinalIgnoreCase) >= 0) return "Cancel requested. Waiting for this step to stop safely...";
            if (line.Contains("Shared game data")) return "Checking shared game data...";
            if (line.Contains("audio") || line.Contains("sound")) return "Preparing optional car sounds...";
            if (line.Contains("tool") || line.Contains("UE Viewer")) return "Verifying extraction tools...";
            if (line.Contains("Extract") || line.Contains("Conver")) return "Extracting and checking your selected assets...";
            if (line.Contains("verified") || line.Contains("Verified")) return "Validating completed files...";
            return null;
        }
        internal static void ParseEndpoint(ref string address, ref int portValue) {
            address = address.Trim(); string candidate = null;
            if (address.StartsWith("[")) {
                int close = address.IndexOf(']'); Guard.Need(close > 1, "Use [IPv6 address]:port or enter address and port separately.");
                string suffix = address.Substring(close + 1); if (suffix.Length > 0) { Guard.Need(suffix.StartsWith(":"), "Use [IPv6 address]:port."); candidate = suffix.Substring(1); }
                address = address.Substring(1, close - 1); IPAddress parsed; Guard.Need(IPAddress.TryParse(address, out parsed), "Enter a valid IPv6 address.");
            } else if (address.IndexOf(':') == address.LastIndexOf(':') && address.Contains(":")) { int split = address.IndexOf(':'); candidate = address.Substring(split + 1); address = address.Substring(0, split); }
            if (candidate != null) { int number; Guard.Need(Int32.TryParse(candidate, out number) && number >= 1024 && number <= 65535, "Use a port from 1024 to 65535."); portValue = number; }
        }
    }
}
