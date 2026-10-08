// Presentation only. Original controls and operation callbacks remain authoritative.
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Text;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Windows.Forms;

namespace SuperRocket64 {
    internal static class ConceptTheme {
        internal static readonly Color Background = Color.FromArgb(10, 23, 35), Surface = Color.FromArgb(13, 29, 43), Border = Color.FromArgb(61, 100, 122), Cyan = Color.FromArgb(48, 224, 239), White = Color.FromArgb(244, 247, 252), Muted = Color.FromArgb(158, 186, 211);
        private static readonly Dictionary<string, Image> images = new Dictionary<string, Image>();
        internal static Image Art(string name) {
            Image result;
            if (!images.TryGetValue(name, out result)) {
                using (var stream = Assembly.GetExecutingAssembly().GetManifestResourceStream("Concept." + name + ".png")) {
                    if (stream == null) throw new InvalidOperationException("Missing launcher artwork: " + name);
                    using (Image source = Image.FromStream(stream)) result = new Bitmap(source);
                }
                images.Add(name, result);
            }
            return result;
        }
        internal static Font Font(float pixels, bool bold) { return new Font("Bahnschrift", pixels, bold ? FontStyle.Bold : FontStyle.Regular, GraphicsUnit.Pixel); }
        internal static GraphicsPath Round(RectangleF r, float radius) {
            float d = Math.Min(radius * 2, Math.Min(r.Width, r.Height)); var p = new GraphicsPath();
            p.AddArc(r.X,r.Y,d,d,180,90); p.AddArc(r.Right-d,r.Y,d,d,270,90); p.AddArc(r.Right-d,r.Bottom-d,d,d,0,90); p.AddArc(r.X,r.Bottom-d,d,d,90,90); p.CloseFigure(); return p;
        }
        internal static void Card(Graphics g, RectangleF r, Color border) {
            using (var path = Round(r,8)) using (var brush = new LinearGradientBrush(r, Color.FromArgb(12,27,41), Color.FromArgb(13,31,48),30f)) using (var pen = new Pen(border,1)) { g.FillPath(brush,path); g.DrawPath(pen,path); }
        }
        internal static void Text(Graphics g,string text,float x,float y,float size,Color color,bool bold) {
            using(var font=Font(size,bold)) using(var brush=new SolidBrush(color)) g.DrawString(text,font,brush,x,y,StringFormat.GenericTypographic);
        }
        internal static void Icon(Graphics g, string icon, RectangleF r, Color color) {
            using(var pen=new Pen(color,Math.Max(1.6f,r.Width/15))) {
                pen.StartCap=pen.EndCap=LineCap.Round; float x=r.X,y=r.Y,w=r.Width,h=r.Height;
                if(icon=="play") { using(var brush=new SolidBrush(color)) g.FillPolygon(brush,new[]{new PointF(x+w*.2f,y+h*.1f),new PointF(x+w*.85f,y+h*.5f),new PointF(x+w*.2f,y+h*.9f)}); }
                else if(icon=="online") { g.DrawEllipse(pen,r);g.DrawEllipse(pen,x+w*.27f,y,w*.46f,h);g.DrawLine(pen,x,y+h*.5f,x+w,y+h*.5f);g.DrawArc(pen,x,y+h*.2f,w,h*.25f,0,180);g.DrawArc(pen,x,y+h*.55f,w,h*.25f,180,180); }
                else if(icon=="setup") { var state=g.Save();g.TranslateTransform(x+w*.5f,y+h*.5f);g.RotateTransform(42);g.DrawLine(pen,0,-h*.16f,0,h*.42f);g.DrawArc(pen,-w*.25f,-h*.42f,w*.5f,h*.43f,0,220);g.Restore(state); }
                else if(icon=="settings") { for(int i=0;i<12;i++){double a=i*Math.PI/6;g.DrawLine(pen,x+w*.5f+(float)Math.Cos(a)*w*.31f,y+h*.5f+(float)Math.Sin(a)*h*.31f,x+w*.5f+(float)Math.Cos(a)*w*.46f,y+h*.5f+(float)Math.Sin(a)*h*.46f);}g.DrawEllipse(pen,x+w*.15f,y+h*.15f,w*.7f,h*.7f);g.DrawEllipse(pen,x+w*.36f,y+h*.36f,w*.28f,h*.28f); }
                else if(icon=="back" || icon=="arrow") { float a=icon=="back"?-1:1;float cx=x+w*.5f;g.DrawLine(pen,x,y+h*.5f,x+w,y+h*.5f);g.DrawLine(pen,cx+a*w*.5f,y+h*.5f,cx,y+h*.1f);g.DrawLine(pen,cx+a*w*.5f,y+h*.5f,cx,y+h*.9f); }
                else if(icon=="check") {g.DrawLines(pen,new[]{new PointF(x,y+h*.5f),new PointF(x+w*.35f,y+h*.85f),new PointF(x+w,y+h*.1f)});}
                else if(icon=="info") {g.DrawEllipse(pen,r);g.DrawLine(pen,x+w*.5f,y+h*.42f,x+w*.5f,y+h*.76f);g.DrawEllipse(pen,x+w*.47f,y+h*.24f,w*.06f,h*.06f);}
            }
        }
    }
    internal sealed class ConceptButton : Button {
        internal bool Primary, Selected, Caption, Navigation;
        internal string IconName, Skin;
        internal Rectangle Source;
        private bool over, pressed;
        internal ConceptButton() { SetStyle(ControlStyles.UserPaint|ControlStyles.AllPaintingInWmPaint|ControlStyles.OptimizedDoubleBuffer|ControlStyles.ResizeRedraw,true); FlatStyle=FlatStyle.Flat;FlatAppearance.BorderSize=0;UseVisualStyleBackColor=false;BackColor=ConceptTheme.Background;ForeColor=ConceptTheme.White;Cursor=Cursors.Hand; }
        protected override void OnMouseEnter(EventArgs e){over=true;Invalidate();base.OnMouseEnter(e);}
        protected override void OnMouseLeave(EventArgs e){over=pressed=false;Invalidate();base.OnMouseLeave(e);}
        protected override void OnMouseDown(MouseEventArgs e){pressed=true;Invalidate();base.OnMouseDown(e);}
        protected override void OnMouseUp(MouseEventArgs e){pressed=false;Invalidate();base.OnMouseUp(e);}
        protected override void OnGotFocus(EventArgs e){Invalidate();base.OnGotFocus(e);}
        protected override void OnLostFocus(EventArgs e){Invalidate();base.OnLostFocus(e);}
        protected override void OnPaint(PaintEventArgs e) {
            Graphics g=e.Graphics;g.SmoothingMode=SmoothingMode.AntiAlias;g.TextRenderingHint=TextRenderingHint.AntiAliasGridFit;g.Clear(BackColor);
            RectangleF r=new RectangleF(.6f,.6f,Width-1.2f,Height-1.2f);
            if(Skin!=null) g.DrawImage(ConceptTheme.Art(Skin),ClientRectangle,Source,GraphicsUnit.Pixel);
            else {
                bool active=Primary||Selected;Color fill=active?ConceptTheme.Cyan:ConceptTheme.Surface;
                if(!Caption||Selected) using(var path=ConceptTheme.Round(r,Math.Max(4,Height*.1f))) using(var brush=new LinearGradientBrush(r,fill,active?Color.FromArgb(63,218,239):Color.FromArgb(16,33,48),0f)) using(var pen=new Pen(active?ConceptTheme.Cyan:ConceptTheme.Border)) {g.FillPath(brush,path);g.DrawPath(pen,path);}
                Color color=!Enabled?Color.FromArgb(108,134,157):active?Color.FromArgb(0,19,28):ConceptTheme.White;
                float size=Font.Size;
                using(var font=ConceptTheme.Font(size,true)) using(var brush=new SolidBrush(color)) using(var format=new StringFormat {Alignment=Navigation?StringAlignment.Near:StringAlignment.Center,LineAlignment=StringAlignment.Center}) {RectangleF label=r;if(Navigation){label.X=Width*.32f;label.Width=Width*.65f;ConceptTheme.Icon(g,IconName,new RectangleF(Width*.10f,Height*.24f,Height*.52f,Height*.52f),color);}else if(IconName!=null){ConceptTheme.Icon(g,IconName,new RectangleF(Width*.045f,Height*.3f,Height*.4f,Height*.4f),color);label.X+=Height*.45f;label.Width-=Height*.45f;}g.DrawString(Text,font,brush,label,format);}
            }
            if(over||pressed||!Enabled) using(var brush=new SolidBrush(!Enabled?Color.FromArgb(100,10,23,35):pressed?Color.FromArgb(45,0,0,0):Color.FromArgb(22,255,255,255))) g.FillRectangle(brush,ClientRectangle);
            if(Focused&&ShowFocusCues) using(var pen=new Pen(Primary||Selected?Color.Black:ConceptTheme.Cyan,2)){pen.DashStyle=DashStyle.Dot;g.DrawRectangle(pen,4,4,Math.Max(1,Width-9),Math.Max(1,Height-9));}
        }
    }
    internal sealed class ConceptCheckBox : CheckBox {
        internal string DisplayText, Skin;
        internal Rectangle Source;
        internal ConceptCheckBox(){SetStyle(ControlStyles.UserPaint|ControlStyles.OptimizedDoubleBuffer|ControlStyles.AllPaintingInWmPaint|ControlStyles.ResizeRedraw|ControlStyles.SupportsTransparentBackColor,true);BackColor=Color.Transparent;ForeColor=ConceptTheme.Muted;}
        protected override void OnPaint(PaintEventArgs e){
            if(Skin!=null){e.Graphics.DrawImage(ConceptTheme.Art(Skin),ClientRectangle,Source,GraphicsUnit.Pixel);float z=Width/(float)Source.Width;if(Checked)ConceptTheme.Icon(e.Graphics,"check",new RectangleF(6*z,10*z,18*z,18*z),ConceptTheme.Cyan);if(Focused)ControlPaint.DrawFocusRectangle(e.Graphics,new Rectangle(1,1,Width-3,Height-3),ConceptTheme.Cyan,ConceptTheme.Background);}
            else PaintChoice(e.Graphics,this,Checked,false,DisplayText??Text);
        }
        internal static void PaintChoice(Graphics g,ButtonBase c,bool value,bool radio,string text) {
            var canvas=c.Parent as ConceptCanvas;if(canvas!=null)canvas.PaintBehindChild(g,c.Location);else g.Clear(c.Parent==null?ConceptTheme.Background:c.Parent.BackColor);
            g.SmoothingMode=SmoothingMode.AntiAlias;g.TextRenderingHint=TextRenderingHint.AntiAliasGridFit;if(c.BackColor.A==255)g.Clear(c.BackColor);float side=Math.Min(c.Height-8,Math.Max(16,c.Font.Size*(radio?1.2f:1f)));RectangleF box=new RectangleF(3,(c.Height-side)/2,side,side);
            using(var pen=new Pen(value?ConceptTheme.Cyan:ConceptTheme.Muted,Math.Max(1.7f,side/12))) {if(radio){g.DrawEllipse(pen,box);if(value)using(var b=new SolidBrush(ConceptTheme.Cyan))g.FillEllipse(b,box.X+side*.25f,box.Y+side*.25f,side*.5f,side*.5f);}else{using(var path=ConceptTheme.Round(box,2)){if(value)using(var b=new SolidBrush(ConceptTheme.Cyan))g.FillPath(b,path);g.DrawPath(pen,path);}if(value)ConceptTheme.Icon(g,"check",new RectangleF(box.X+side*.2f,box.Y+side*.2f,side*.6f,side*.6f),ConceptTheme.Background);}}
            using(var brush=new SolidBrush(c.Enabled?c.ForeColor:Color.Gray))using(var format=new StringFormat{LineAlignment=StringAlignment.Center})g.DrawString(text,c.Font,brush,new RectangleF(side+23,0,Math.Max(1,c.Width-side-23),c.Height),format);
            if(c.Focused)ControlPaint.DrawFocusRectangle(g,new Rectangle(1,1,c.Width-3,c.Height-3),ConceptTheme.Cyan,c.BackColor);
        }
    }
    internal sealed class ConceptRadioButton : RadioButton {
        internal ConceptRadioButton(){SetStyle(ControlStyles.UserPaint|ControlStyles.OptimizedDoubleBuffer|ControlStyles.AllPaintingInWmPaint|ControlStyles.ResizeRedraw|ControlStyles.SupportsTransparentBackColor,true);BackColor=Color.Transparent;ForeColor=ConceptTheme.White;}
        protected override void OnPaint(PaintEventArgs e){ConceptCheckBox.PaintChoice(e.Graphics,this,Checked,true,Text);}
    }
    internal sealed class ConceptLabel : Label {
        protected override void OnPaint(PaintEventArgs e){e.Graphics.TextRenderingHint=TextRenderingHint.AntiAliasGridFit;using(var brush=new SolidBrush(ForeColor))using(var format=new StringFormat{Alignment=TextAlign==ContentAlignment.MiddleCenter?StringAlignment.Center:StringAlignment.Near,LineAlignment=TextAlign==ContentAlignment.MiddleCenter?StringAlignment.Center:StringAlignment.Near})e.Graphics.DrawString(Text,Font,brush,ClientRectangle,format);}
    }
    internal sealed class ConceptComboBox : ComboBox {
        internal ConceptComboBox(){DrawMode=DrawMode.OwnerDrawFixed;BackColor=ConceptTheme.Background;ForeColor=ConceptTheme.White;FlatStyle=FlatStyle.Flat;}
        protected override void OnDrawItem(DrawItemEventArgs e){if(e.Bounds.Width<1)return;using(var brush=new SolidBrush(ConceptTheme.Background))e.Graphics.FillRectangle(brush,e.Bounds);e.Graphics.TextRenderingHint=TextRenderingHint.AntiAliasGridFit;if(e.Index>=0&&e.Index<Items.Count)using(var brush=new SolidBrush(ConceptTheme.White))e.Graphics.DrawString(GetItemText(Items[e.Index]),Font,brush,e.Bounds);if((e.State&DrawItemState.Focus)!=0)e.DrawFocusRectangle();}
        protected override void OnFontChanged(EventArgs e){base.OnFontChanged(e);ItemHeight=Math.Max(18,Font.Height+8);}
    }
    internal sealed class ConceptArt : Control {
        private readonly string art; private readonly Rectangle source;
        internal ConceptArt(string name,Rectangle area,string accessible){art=name;source=area;AccessibleName=accessible;AccessibleRole=AccessibleRole.Graphic;TabStop=false;SetStyle(ControlStyles.UserPaint|ControlStyles.OptimizedDoubleBuffer,true);}
        protected override void OnPaint(PaintEventArgs e){e.Graphics.InterpolationMode=InterpolationMode.HighQualityBicubic;e.Graphics.DrawImage(ConceptTheme.Art(art),ClientRectangle,source,GraphicsUnit.Pixel);}
    }
    internal sealed class ConceptCanvas : Panel {
        internal float Zoom=1;
        internal Action<Graphics> PaintDesign;
        internal ConceptCanvas(){BackColor=ConceptTheme.Background;Margin=Padding.Empty;SetStyle(ControlStyles.UserPaint|ControlStyles.OptimizedDoubleBuffer|ControlStyles.AllPaintingInWmPaint|ControlStyles.ResizeRedraw,true);}
        // RadioButton/CheckBox themed background painting can leave a black buffer
        // during WM_PRINT and normal repaints. Paint the actual parent surface in
        // the child's coordinate space; do not rely on inherited transparency.
        internal void PaintBehindChild(Graphics graphics,Point location){var state=graphics.Save();try{graphics.TranslateTransform(-location.X,-location.Y);using(var brush=new SolidBrush(BackColor))graphics.FillRectangle(brush,ClientRectangle);OnPaint(new PaintEventArgs(graphics,ClientRectangle));}finally{graphics.Restore(state);}}
        protected override void OnPaint(PaintEventArgs e){base.OnPaint(e);var state=e.Graphics.Save();e.Graphics.ScaleTransform(Zoom,Zoom);e.Graphics.SmoothingMode=SmoothingMode.AntiAlias;e.Graphics.TextRenderingHint=TextRenderingHint.AntiAliasGridFit;if(PaintDesign!=null)PaintDesign(e.Graphics);e.Graphics.Restore(state);}
    }
    internal sealed class ConceptField : Panel {
        private readonly TextBox input;
        internal ConceptField(TextBox text){input=text;text.BorderStyle=BorderStyle.None;text.BackColor=ConceptTheme.Background;text.ForeColor=ConceptTheme.White;Controls.Add(text);BackColor=ConceptTheme.Background;SetStyle(ControlStyles.UserPaint|ControlStyles.OptimizedDoubleBuffer|ControlStyles.ResizeRedraw,true);}
        protected override void OnLayout(LayoutEventArgs e){base.OnLayout(e);int padding=Math.Max(10,Height/3);input.Bounds=new Rectangle(padding,Math.Max(2,(Height-input.PreferredHeight)/2),Math.Max(1,Width-padding*2),input.PreferredHeight);}
        protected override void OnPaint(PaintEventArgs e){base.OnPaint(e);e.Graphics.SmoothingMode=SmoothingMode.AntiAlias;using(var path=ConceptTheme.Round(new RectangleF(.5f,.5f,Width-1,Height-1),6))using(var pen=new Pen(input.Focused?ConceptTheme.Cyan:ConceptTheme.Muted))e.Graphics.DrawPath(pen,path);}
    }
    internal sealed partial class LauncherForm {
        private ConceptCanvas chrome;
        private readonly Dictionary<FlowLayoutPanel,ConceptCanvas> concepts=new Dictionary<FlowLayoutPanel,ConceptCanvas>();
        private readonly Dictionary<ConceptCanvas,Action> conceptLayouts=new Dictionary<ConceptCanvas,Action>();
        private readonly List<Font> conceptFonts=new List<Font>();
        private readonly Dictionary<string,Font> fontCache=new Dictionary<string,Font>();
        private readonly ConceptButton[] navigation=new ConceptButton[4];
        private ConceptButton minimizeCaption,maximizeCaption,closeCaption;
        private float conceptScale=1;
        private bool arrangingConcept;
        private int conceptOrder;
        [DllImport("user32.dll")] private static extern bool ReleaseCapture();
        [DllImport("user32.dll")] private static extern IntPtr SendMessage(IntPtr window,int message,IntPtr wParam,IntPtr lParam);
        private Font UiFont(float pixels,bool bold) {pixels=(float)Math.Round(pixels*4)/4;string key=pixels.ToString(System.Globalization.CultureInfo.InvariantCulture)+":"+bold;Font font;if(!fontCache.TryGetValue(key,out font)){font=ConceptTheme.Font(pixels,bold);fontCache.Add(key,font);conceptFonts.Add(font);}return font; }
        private static Button FindButton(Control parent,string label){foreach(Control c in parent.Controls){Button b=c as Button;if(b!=null&&b.Text==label)return b;b=FindButton(c,label);if(b!=null)return b;}return null;}
        private void Place(Control c,Control parent,float x,float y,float w,float h,float fontSize) {
            if(c.Parent!=parent)parent.Controls.Add(c);c.AutoSize=false;c.Dock=DockStyle.None;c.Margin=Padding.Empty;c.MinimumSize=Size.Empty;c.MaximumSize=Size.Empty;
            float s=conceptScale;c.SetBounds((int)Math.Round(x*s),(int)Math.Round(y*s),(int)Math.Round(w*s),(int)Math.Round(h*s));
            float size=Math.Max(12,fontSize*s);if(Math.Abs(c.Font.Size-size)>.15f||c.Font.Unit!=GraphicsUnit.Pixel)c.Font=UiFont(size,c is Button);
            ConceptField field=c as ConceptField;if(field!=null){foreach(Control child in field.Controls)child.Font=c.Font;field.PerformLayout();}
            c.TabIndex=conceptOrder++;
        }
        private Label CopyLabel(string text,bool bright){return new ConceptLabel{Text=text,BackColor=Color.Transparent,ForeColor=bright?ConceptTheme.White:ConceptTheme.Muted,UseMnemonic=false};}
        private ConceptCanvas Canvas(FlowLayoutPanel page){page.Controls.Clear();page.AutoSize=false;page.Dock=DockStyle.None;page.Location=Point.Empty;page.Padding=Padding.Empty;page.Margin=Padding.Empty;page.BackColor=ConceptTheme.Background;var canvas=new ConceptCanvas();page.Controls.Add(canvas);concepts.Add(page,canvas);return canvas;}
        private void Header(ConceptCanvas canvas,string title,string subtitle,string art,Rectangle crop,float w,float h) {
            Control heading=art==null?(Control)CopyLabel(title,true):new ConceptArt(art,crop,title);var sub=CopyLabel(subtitle,false);
            canvas.Controls.Add(heading);canvas.Controls.Add(sub);
            Action prior;conceptLayouts.TryGetValue(canvas,out prior);conceptLayouts[canvas]=delegate {if(prior!=null)prior();Place(heading,canvas,0,0,w,h,60);Place(sub,canvas,5,76,1190,42,30);};
        }
        private void AddLayout(ConceptCanvas canvas,Action action){Action old;conceptLayouts.TryGetValue(canvas,out old);conceptLayouts[canvas]=delegate{if(old!=null)old();action();};}
        private float CopyHeight(string text,float width,float size,float minimum){using(var bitmap=new Bitmap(1,1))using(var g=Graphics.FromImage(bitmap))using(var font=ConceptTheme.Font(Math.Max(12,size*conceptScale)/conceptScale,false))return Math.Max(minimum,(float)Math.Ceiling(g.MeasureString(text,font,(int)width).Height)+8);}
        private void PageHeight(ConceptCanvas canvas,float height){canvas.Size=new Size((int)Math.Round(1198*conceptScale),(int)Math.Ceiling(height*conceptScale));canvas.Zoom=conceptScale;}
        private void Primary(Button button){ConceptButton b=button as ConceptButton;if(b!=null)b.Primary=true;}
        private void Skin(Button button,string art,Rectangle rect){var b=button as ConceptButton;if(b!=null){b.Skin=art;b.Source=rect;}}
        private void InitializePresentation() {
            SuspendLayout();MaximizedBounds=Screen.FromControl(this).WorkingArea;AutoScaleMode=AutoScaleMode.None;FormBorderStyle=FormBorderStyle.None;float displayScale;using(var display=Graphics.FromHwnd(IntPtr.Zero))displayScale=display.DpiX/96f;MinimumSize=new Size((int)(960*displayScale),(int)(640*displayScale));BackColor=ConceptTheme.Background;
            Size work=Screen.FromControl(this).WorkingArea.Size;MinimumSize=new Size(Math.Min(MinimumSize.Width,work.Width),Math.Min(MinimumSize.Height,work.Height));ClientSize=new Size(Math.Min((int)(1152*displayScale),work.Width-60),Math.Min((int)(768*displayScale),work.Height-60));
            foreach(Control c in new Control[]{pageHost,notice,cancelOperation})if(c.Parent!=null)c.Parent.Controls.Remove(c);
            Controls.Clear();chrome=new ConceptCanvas{Dock=DockStyle.Fill};Controls.Add(chrome);chrome.Controls.Add(pageHost);chrome.Controls.Add(notice);
            pageHost.Dock=DockStyle.None;pageHost.BackColor=ConceptTheme.Background;pageHost.AutoScroll=true;notice.BackColor=ConceptTheme.Surface;notice.ForeColor=Color.FromArgb(255,208,87);notice.AutoSize=false;notice.MaximumSize=Size.Empty;
            string[] titles={"PLAY","ONLINE","SETUP","SETTINGS"};string[] icons={"play","online","setup","settings"};
            Action[] actions={delegate{ShowPage(installationReady?homePage:locationPage);},delegate{ShowPage(onlineChoicePage);},ShowSetupPage,ShowUpdateSettings};
            for(int i=0;i<4;i++){int j=i;var b=new ConceptButton{Text=titles[i],IconName=icons[i],AccessibleName=titles[i],Caption=true,Navigation=true};b.Click+=delegate{if(!running)actions[j]();};navigation[i]=b;chrome.Controls.Add(b);}
            minimizeCaption=new ConceptButton{Text="—",Caption=true,AccessibleName="Minimize window"};maximizeCaption=new ConceptButton{Text="□",Caption=true,AccessibleName="Maximize or restore window"};closeCaption=new ConceptButton{Text="×",Caption=true,AccessibleName="Close launcher"};
            minimizeCaption.Click+=delegate{WindowState=FormWindowState.Minimized;};maximizeCaption.Click+=delegate{WindowState=WindowState==FormWindowState.Maximized?FormWindowState.Normal:FormWindowState.Maximized;};closeCaption.Click+=delegate{Close();};
            Skin(minimizeCaption,"01-play",new Rectangle(1350,3,58,47));Skin(maximizeCaption,"01-play",new Rectangle(1410,3,58,47));Skin(closeCaption,"01-play",new Rectangle(1470,3,58,47));
            chrome.Controls.Add(minimizeCaption);chrome.Controls.Add(maximizeCaption);chrome.Controls.Add(closeCaption);
            chrome.MouseDown+=delegate(object sender,MouseEventArgs e){if(e.Button==MouseButtons.Left&&e.Y<52*conceptScale){ReleaseCapture();SendMessage(Handle,0xA1,new IntPtr(2),IntPtr.Zero);}};
            chrome.PaintDesign=PaintChrome;
            BuildHomeConcept();BuildLocationConcept();BuildSettingsConcept();BuildOnlineConcept();BuildSourcesConcept();BuildExtrasConcept();BuildOperationConcepts();
            SizeChanged+=delegate{ArrangeConcept();};notice.TextChanged+=delegate{ArrangeConcept();};cancelOperation.VisibleChanged+=delegate{ArrangeConcept();};pageHost.EnabledChanged+=delegate{foreach(var b in navigation)b.Enabled=pageHost.Enabled;};
            ResumeLayout();ArrangeConcept();
        }
        private void PaintChrome(Graphics g) {
            float width=ClientSize.Width/conceptScale,height=ClientSize.Height/conceptScale;
            using(var brush=new LinearGradientBrush(new RectangleF(0,0,width,height),Color.FromArgb(12,25,38),ConceptTheme.Background,20f))g.FillRectangle(brush,0,0,width,height);
            g.DrawImage(ConceptTheme.Art("01-play"),new RectangleF(0,0,1536,52),new RectangleF(0,0,1536,52),GraphicsUnit.Pixel);
            g.DrawImage(ConceptTheme.Art("01-play"),new RectangleF(0,52,261,550),new RectangleF(0,52,261,550),GraphicsUnit.Pixel);
            g.DrawImage(ConceptTheme.Art("01-play"),new RectangleF(0,602,261,Math.Max(1,height-644)),new RectangleF(0,602,261,339),GraphicsUnit.Pixel);
            using(var p=new Pen(ConceptTheme.Border)){g.DrawLine(p,0,52,width,52);g.DrawLine(p,261,52,261,height-42);g.DrawLine(p,0,height-42,width,height-42);g.DrawRectangle(p,0,0,width-1,height-1);}
            ConceptTheme.Text(g,"WINDOWS LAUNCHER",42,height-73,15,ConceptTheme.Muted,false);
            ConceptTheme.Text(g,"SUPER ROCKET 64  ·  "+UpdateBuild.DisplayVersion,23,height-28,14,ConceptTheme.Muted,false);
            ConceptTheme.Text(g,"Game sources stay on this PC",width-284,height-28,14,ConceptTheme.Muted,false);
        }
        private void ArrangeConcept() {
            if(chrome==null||arrangingConcept||ClientSize.Width<1)return;arrangingConcept=true;
            try {
                conceptScale=ClientSize.Width/1536f;chrome.Zoom=conceptScale;conceptOrder=0;
                int footer=(int)(42*conceptScale),noticeHeight=String.IsNullOrWhiteSpace(notice.Text)?0:(int)(CopyHeight(notice.Text,1198,20,58)*conceptScale);
                int operationHeight=cancelOperation.Visible?(int)(90*conceptScale):0;
                pageHost.SetBounds((int)Math.Round(300*conceptScale),(int)Math.Round(86*conceptScale),(int)Math.Round(1198*conceptScale)+SystemInformation.VerticalScrollBarWidth,Math.Max(80,ClientSize.Height-footer-(int)Math.Round(86*conceptScale)-noticeHeight-operationHeight));
                notice.Visible=noticeHeight>0;Place(notice,chrome,300,(ClientSize.Height-footer-noticeHeight)/conceptScale,1198,noticeHeight/conceptScale,20);
                for(int i=0;i<4;i++)Place(navigation[i],chrome,11,345+i*66,243,59,26);
                Place(minimizeCaption,chrome,1350,3,58,47,29);Place(maximizeCaption,chrome,1410,3,58,47,27);Place(closeCaption,chrome,1470,3,58,47,36);
                foreach(var entry in concepts){var canvas=entry.Value;conceptOrder=0;Action layout;if(conceptLayouts.TryGetValue(canvas,out layout))layout();entry.Key.Width=canvas.Width;entry.Key.Height=canvas.Height;}
                chrome.Invalidate();foreach(var c in concepts.Values)c.Invalidate();
            } finally {arrangingConcept=false;}
        }
        private void SelectConcept(FlowLayoutPanel page) {
            if(chrome==null)return;int selected=page==homePage?0:(page==onlinePage||page==onlineChoicePage)?1:(page==settingsPage||page==updatePage)?3:2;
            for(int i=0;i<4;i++){var b=navigation[i];b.Selected=i==selected;b.Skin=(i==0&&b.Selected)||(i>0&&!b.Selected)?"01-play":null;b.Source=new Rectangle(11,345+i*66,243,59);b.Invalidate();}
            CancelButton=page==progressPage?cancelOperation:FindButton(page,"Back")??FindButton(page,"Cancel")??(page==failurePage?failureBack:null);
            ArrangeConcept();pageHost.AutoScrollPosition=Point.Empty;if(IsHandleCreated)BeginInvoke(new Action(delegate{if(!IsDisposed&&page.Visible)page.SelectNextControl(null,true,true,true,false);}));
        }
        [StructLayout(LayoutKind.Sequential)] private struct DpiBounds { internal int Left,Top,Right,Bottom; }
        protected override void WndProc(ref Message m) {
            if(m.Msg==0x02E0){var r=(DpiBounds)Marshal.PtrToStructure(m.LParam,typeof(DpiBounds));float dpi=((long)m.WParam&65535)/96f;var proposed=new Rectangle(r.Left,r.Top,r.Right-r.Left,r.Bottom-r.Top);Rectangle area=Screen.FromRectangle(proposed).WorkingArea;MaximizedBounds=area;MinimumSize=new Size(Math.Min((int)(960*dpi),area.Width),Math.Min((int)(640*dpi),area.Height));int width=Math.Min(proposed.Width,area.Width),height=Math.Min(proposed.Height,area.Height);Bounds=new Rectangle(Math.Max(area.Left,Math.Min(r.Left,area.Right-width)),Math.Max(area.Top,Math.Min(r.Top,area.Bottom-height)),width,height);ArrangeConcept();m.Result=IntPtr.Zero;return;}

            if(m.Msg==0x84&&FormBorderStyle==FormBorderStyle.None&&WindowState!=FormWindowState.Maximized){base.WndProc(ref m);Point p=PointToClient(new Point(unchecked((short)((long)m.LParam&65535)),unchecked((short)(((long)m.LParam>>16)&65535))));int grip=7;bool l=p.X<grip,r=p.X>=Width-grip,t=p.Y<grip,b=p.Y>=Height-grip;if(l||r||t||b){m.Result=new IntPtr(t?(l?13:r?14:12):b?(l?16:r?17:15):l?10:11);return;}return;}base.WndProc(ref m);
        }
        protected override void Dispose(bool disposing){if(disposing){foreach(Font f in conceptFonts)f.Dispose();conceptFonts.Clear();}base.Dispose(disposing);}
        private void BuildHomeConcept() {
            Button play=FindButton(homePage,"Play Offline"),online=FindButton(homePage,"Online"),setup=FindButton(homePage,"Setup / repair / add characters");
            var canvas=Canvas(homePage);Header(canvas,"Ready for liftoff.","Your next run starts here.","01-play",new Rectangle(300,86,555,73),555,73);
            canvas.Controls[0].Visible=false;canvas.Controls[1].Visible=false;
            var homeMute=(ConceptCheckBox)mute;homeMute.Skin="01-play";homeMute.Source=new Rectangle(300,806,1000,44);
            var hero=new ConceptArt("01-play",new Rectangle(300,213,1198,445),"Super Rocket 64 — cyan Octane boosting over floating tracks");var hint=new ConceptArt("01-play",new Rectangle(302,770,1000,36),"Choose your character in-game.");
            Skin(play,"01-play",new Rectangle(300,678,597,82));Skin(online,"01-play",new Rectangle(916,678,582,82));Skin(setup,"01-play",new Rectangle(302,881,1196,70));
            canvas.PaintDesign=delegate(Graphics g){
                var state=g.Save();g.ExcludeClip(new Rectangle(985,18,213,67));g.DrawImage(ConceptTheme.Art("01-play"),new Rectangle(0,0,1198,881),new Rectangle(300,86,1198,881),GraphicsUnit.Pixel);g.Restore(state);
                // The one requested removal: reconstruct only the empty background beneath the readiness pill.
                g.DrawImage(ConceptTheme.Art("01-play"),new Rectangle(985,18,213,67),new Rectangle(1065,104,213,67),GraphicsUnit.Pixel);
            };
            AddLayout(canvas,delegate{PageHeight(canvas,881);Place(hero,canvas,0,127,1198,445,20);Place(play,canvas,0,592,597,82,32);Place(online,canvas,616,592,582,82,32);Place(hint,canvas,2,684,1000,36,21);Place(mute,canvas,0,720,1000,44,21);Place(setup,canvas,2,795,1196,70,25);});
        }
        private void BuildLocationConcept() {
            Button browse=FindButton(install.Parent,"Browse..."),next=FindButton(locationPage,"Next"),cancel=FindButton(locationPage,"Cancel");var field=new ConceptField(install);var canvas=Canvas(locationPage);
            Header(canvas,"Choose where to install","1 of 5  ·  Installation folder","02-install-location",new Rectangle(300,86,810,73),810,73);
            var title=new ConceptArt("02-install-location",new Rectangle(329,348,741,50),"One folder. Everything together.");var folderArt=new ConceptArt("02-install-location",new Rectangle(1070,331,422,302),"Folder with cyan car and floating island");
            var description=CopyLabel("Program files, reusable assets, saves and controls.",false);var label=CopyLabel("Installation folder",true);var help=CopyLabel("Optional characters may need more space. Rocket League is not copied in full.",false);
            Primary(next);canvas.PaintDesign=delegate(Graphics g){Stepper(g,1);ConceptTheme.Card(g,new RectangleF(0,220,1198,330),ConceptTheme.Border);ConceptTheme.Card(g,new RectangleF(0,571,573,129),ConceptTheme.Border);ConceptTheme.Card(g,new RectangleF(594,571,604,129),ConceptTheme.Border);ConceptTheme.Text(g,"1 GB free",140,596,28,ConceptTheme.White,true);ConceptTheme.Text(g,"Allow space for program files\nand temporary work.",140,635,23,ConceptTheme.Muted,false);ConceptTheme.Icon(g,"settings",new RectangleF(34,610,44,44),ConceptTheme.Cyan);ConceptTheme.Text(g,"Have an installation?",746,596,28,ConceptTheme.White,true);ConceptTheme.Text(g,"Choose its folder to update or repair.",746,635,23,ConceptTheme.Muted,false);ConceptTheme.Icon(g,"setup",new RectangleF(628,610,44,44),ConceptTheme.Cyan);using(var p=new Pen(ConceptTheme.Border))g.DrawLine(p,0,772,1198,772);};
            AddLayout(canvas,delegate{PageHeight(canvas,881);Place(title,canvas,29,262,741,50,40);Place(description,canvas,31,318,730,45,30);Place(folderArt,canvas,770,245,422,302,20);Place(label,canvas,32,388,710,38,25);Place(field,canvas,32,431,519,68,28);Place(browse,canvas,564,431,174,68,28);Place(help,canvas,48,716,1148,40,21);Place(cancel,canvas,2,795,226,70,28);Place(next,canvas,909,795,289,70,28);});
        }
        private void Stepper(Graphics g,int active){string[] labels={"Location","Games","Characters","Install","Ready"};for(int i=0;i<5;i++){float x=127+i*233;if(i<4)using(var p=new Pen(i+1<active?ConceptTheme.Cyan:ConceptTheme.Border,2))g.DrawLine(p,x+22,146,x+210,146);using(var p=new Pen(i+1<=active?ConceptTheme.Cyan:ConceptTheme.Muted,3))g.DrawEllipse(p,x-13,133,26,26);if(i+1<=active)using(var b=new SolidBrush(ConceptTheme.Cyan))g.FillEllipse(b,x-8,138,16,16);ConceptTheme.Text(g,labels[i],x-47,170,24,i+1==active?ConceptTheme.Cyan:ConceptTheme.Muted,i+1==active);}}
        private void BuildSettingsConcept() {
            Button save=FindButton(settingsPage,"Save preferences"),check=FindButton(settingsPage,"Check for updates"),remove=FindButton(settingsPage,"Remove launcher shortcuts"),back=FindButton(settingsPage,"Back");var canvas=Canvas(settingsPage);
            Header(canvas,"Make it yours.","Updates & shortcuts.","03-settings",new Rectangle(300,86,495,67),495,67);
            var updateTitle=new ConceptArt("03-settings",new Rectangle(336,229,394,39),"Launcher updates");var shortcutTitle=new ConceptArt("03-settings",new Rectangle(338,592,254,36),"Shortcuts");
            var autoHelp=CopyLabel("Check at startup, verify and apply before play.",false);var manualHelp=CopyLabel("No startup update checks. Check when you choose.",false);var previews=CopyLabel("Preview releases are included.",false);var safe=CopyLabel("Assets, saves and controller settings stay in the data folder.",false);
            settingsMessage.BackColor=ConceptTheme.Background;settingsMessage.ForeColor=ConceptTheme.Muted;Primary(save);
            canvas.PaintDesign=delegate(Graphics g){ConceptTheme.Card(g,new RectangleF(0,124,1198,352),ConceptTheme.Border);ConceptTheme.Card(g,new RectangleF(0,494,1198,175),ConceptTheme.Border);};
            AddLayout(canvas,delegate{bool detailed=settingsMessage.Text.Length>120;float extra=detailed?80:0;PageHeight(canvas,895+extra);Place(updateTitle,canvas,36,143,394,39,40);Place(automaticUpdates,canvas,36,193,1100,40,28);Place(autoHelp,canvas,99,239,1000,32,24);Place(manualUpdates,canvas,36,283,1100,40,28);Place(manualHelp,canvas,99,329,1040,32,24);Place(previews,canvas,91,367,980,27,21);Place(check,canvas,39,405,345,57,26);Place(shortcutTitle,canvas,38,520,254,36,38);Place(desktopShortcut,canvas,36,565,1000,46,27);Place(menuShortcut,canvas,36,614,1000,46,27);Place(rollbackUpdate,canvas,1,687,573,70,24);Place(remove,canvas,592,687,606,70,24);Place(safe,canvas,46,764,1140,33,20);Place(settingsMessage,canvas,46,795,1140,28+extra,17);Place(back,canvas,2,824+extra,226,67,27);Place(save,canvas,909,824+extra,289,67,27);});
        }
        private void BuildOnlineConcept() {
            Button chooseHost=FindButton(onlineChoicePage,"Host"),chooseJoin=FindButton(onlineChoicePage,"Join"),backChoice=FindButton(onlineChoicePage,"Back"),back=FindButton(onlinePage,"Back"),paste=FindButton(joinAddressRow,"Paste");
            var choice=Canvas(onlineChoicePage);Header(choice,"Play together.","LAN or an existing Tailscale connection.","04-online-states",new Rectangle(218,65,365,40),730,80);
            var hostHelp=CopyLabel("Share an address your\nfriends can reach.",false);var joinHelp=CopyLabel("Connect to your\nfriend's host.",false);var networkHelp=CopyLabel("Configure Tailscale before playing. The launcher does not change network or firewall settings.",false);
            Primary(chooseHost);Primary(chooseJoin);choice.PaintDesign=delegate(Graphics g){ConceptTheme.Card(g,new RectangleF(0,157,576,540),ConceptTheme.Cyan);ConceptTheme.Card(g,new RectangleF(609,157,588,540),ConceptTheme.Cyan);g.DrawImage(ConceptTheme.Art("04-online-states"),new Rectangle(189,206,174,160),new Rectangle(299,164,95,88),GraphicsUnit.Pixel);g.DrawImage(ConceptTheme.Art("04-online-states"),new Rectangle(804,206,174,160),new Rectangle(564,164,95,88),GraphicsUnit.Pixel);g.DrawImage(ConceptTheme.Art("04-online-states"),new Rectangle(45,395,334,59),new Rectangle(240,259,168,30),GraphicsUnit.Pixel);g.DrawImage(ConceptTheme.Art("04-online-states"),new Rectangle(655,395,326,59),new Rectangle(514,261,163,30),GraphicsUnit.Pixel);};
            AddLayout(choice,delegate{PageHeight(choice,881);Place(hostHelp,choice,45,463,480,92,32);Place(joinHelp,choice,655,463,470,92,32);Place(chooseHost,choice,45,587,485,85,32);Place(chooseJoin,choice,655,587,497,85,32);Place(networkHelp,choice,46,717,1150,65,21);Place(backChoice,choice,0,810,250,67,27);});
            onlineCharacter.BackColor=shareAddresses.BackColor=port.BackColor=ConceptTheme.Background;
            onlineCharacter.ForeColor=shareAddresses.ForeColor=port.ForeColor=ConceptTheme.White;
            onlineCharacter.FlatStyle=shareAddresses.FlatStyle=FlatStyle.Flat;
            var nameBox=new ConceptField(player);var hostBox=new ConceptField(host);var details=Canvas(onlinePage);Header(details,"Host a game","Share an address your friends can reach.",null,Rectangle.Empty,1100,73);
            Control dynamicHeading=details.Controls[0];dynamicHeading.Visible=false;Label dynamicSubtitle=(Label)details.Controls[1];
            var hostHeading=new ConceptArt("04-online-states",new Rectangle(968,61,277,43),"Host a game");
            var joinHeading=new ConceptArt("04-online-states",new Rectangle(218,568,247,43),"Join a game");var characterLabel=CopyLabel("Character",true);var portLabel=CopyLabel("Port",true);var nameLabel=CopyLabel("Your name (optional)",true);var ownLabel=CopyLabel("This PC's LAN / Tailscale addresses",true);var friendLabel=CopyLabel("Friend's host address",true);var advice=CopyLabel("Each player needs the same build and their own Octane setup. Online supports Mario and Octane.",false);
            hostAddressRow.Controls.Clear();joinAddressRow.Controls.Clear();hostAddressRow.AutoSize=false;joinAddressRow.AutoSize=false;hostAddressRow.Padding=joinAddressRow.Padding=Padding.Empty;hostAddressRow.BackColor=joinAddressRow.BackColor=ConceptTheme.Surface;
            var hostPanel=new Panel{BackColor=ConceptTheme.Surface};var joinPanel=new Panel{BackColor=ConceptTheme.Surface};
            Primary(startHost);Primary(joinGame);details.PaintDesign=delegate(Graphics g){ConceptTheme.Card(g,new RectangleF(0,151,1198,285),ConceptTheme.Border);ConceptTheme.Card(g,new RectangleF(0,463,1198,264),ConceptTheme.Border);};
            AddLayout(details,delegate{bool hosting=onlineMode!="join";PageHeight(details,881);Place(hostHeading,details,0,0,471,73,40);Place(joinHeading,details,0,0,419,73,40);hostHeading.Visible=hosting;joinHeading.Visible=!hosting;dynamicSubtitle.Text=hosting?"Share an address your friends can reach.":"Connect to your friend's host.";Place(characterLabel,details,38,181,320,50,27);Place(onlineCharacter,details,413,179,540,58,28);Place(portLabel,details,38,264,320,50,27);Place(port,details,413,261,238,60,28);Place(nameLabel,details,38,348,357,50,27);Place(nameBox,details,413,342,743,65,27);Place(hostAddressRow,details,34,484,1130,181,24);Place(joinAddressRow,details,34,484,1130,181,24);hostAddressRow.Visible=hosting;joinAddressRow.Visible=!hosting;
                // Rows retain their original visibility contract; child coordinates are independent of FlowLayout.
                hostAddressRow.FlowDirection=FlowDirection.LeftToRight;hostAddressRow.WrapContents=true;joinAddressRow.FlowDirection=FlowDirection.LeftToRight;joinAddressRow.WrapContents=true;
                Place(hostPanel,hostAddressRow,0,0,1130,181,24);Place(joinPanel,joinAddressRow,0,0,1130,181,24);
                ownLabel.AutoSize=false;Place(ownLabel,hostPanel,0,0,1100,45,26);Place(shareAddresses,hostPanel,0,50,1100,57,26);Place(copyAddress,hostPanel,0,117,535,56,24);Place(refreshAddresses,hostPanel,553,117,547,56,24);
                Place(friendLabel,joinPanel,0,0,1100,45,26);Place(hostBox,joinPanel,0,54,802,69,26);Place(paste,joinPanel,823,54,277,69,26);
                Place(addressStatus,details,38,670,1117,48,18);addressStatus.Visible=hosting;addressStatus.ForeColor=ConceptTheme.Muted;addressStatus.BackColor=ConceptTheme.Surface;Place(advice,details,38,739,1120,57,21);Place(back,details,0,810,250,67,27);Place(startHost,details,793,810,405,67,27);Place(joinGame,details,793,810,405,67,27);
            });
        }
    }
}
