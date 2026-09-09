using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Security.Principal;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.Runtime.InteropServices;
[assembly: System.Reflection.AssemblyTitle("虎娘卸载")]
[assembly: System.Reflection.AssemblyProduct("Tigirl")]
[assembly: System.Reflection.AssemblyVersion("2026.9.10.2")]
class Maintenance : Form {
 [DllImport("user32.dll")] static extern bool SetProcessDPIAware();
 readonly CheckBox erase = new CheckBox { Text="同时删除当前用户的码表、设置和个人词条（包括备份）",AutoSize=true };
 readonly Button uninstall = new Button {Text="卸载", AutoSize=true};
 readonly Label status = new Label {AutoSize=true,MaximumSize=new Size(560,0)};
 readonly string data=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"NativeTiger");
 readonly string root=AppDomain.CurrentDomain.BaseDirectory;
 bool busy, complete;
 static bool Elevated(){return new WindowsPrincipal(WindowsIdentity.GetCurrent()).IsInRole(WindowsBuiltInRole.Administrator);}
 [STAThread] static void Main(string[] args) {
  SetProcessDPIAware(); Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);
  if(args.Length>0){return;} // No command line can request destructive user-data cleanup.
  Application.Run(new Maintenance());
 }
 Maintenance(){
  Text="卸载虎娘";AutoScaleDimensions=new SizeF(96F,96F);AutoScaleMode=AutoScaleMode.Dpi;Font=new Font("Microsoft YaHei UI",9F);ClientSize=new Size(610,290);StartPosition=FormStartPosition.CenterScreen;MaximizeBox=false;FormBorderStyle=FormBorderStyle.FixedDialog;
  Icon=Icon.ExtractAssociatedIcon(Application.ExecutablePath);
  var panel=new FlowLayoutPanel{Dock=DockStyle.Fill,FlowDirection=FlowDirection.TopDown,Padding=new Padding(22),WrapContents=false};Controls.Add(panel);
  panel.Controls.Add(new Label{Text="卸载虎娘 / Tigirl",Font=new Font(Font.FontFamily,16),AutoSize=true,Margin=new Padding(0,0,0,16)});
  panel.Controls.Add(new Label{Text="将移除输入法及程序文件。默认保留码表和个人设置。",AutoSize=true});
  panel.Controls.Add(erase);panel.Controls.Add(new Label{Text=data,AutoSize=true,MaximumSize=new Size(560,0)});
  if(Elevated()){erase.Enabled=false;status.Text="当前以管理员身份运行，仅允许保留用户数据卸载。";}
  panel.Controls.Add(status);panel.Controls.Add(uninstall);uninstall.Click+=async (s,e)=>await Remove();
  FormClosing+=(s,e)=>{if(busy)e.Cancel=true;};
 }
 static void AssertPlain(string path){
  var node=new DirectoryInfo(path);
  while(node!=null){if(node.Exists&&(node.Attributes&FileAttributes.ReparsePoint)!=0)throw new IOException("数据目录被重定向，已停止删除。");node=node.Parent;}
  if(!Directory.Exists(path))return;
  foreach(var entry in Directory.GetFileSystemEntries(path)){
   var attr=File.GetAttributes(entry);if((attr&FileAttributes.ReparsePoint)!=0)throw new IOException("数据目录含链接，已停止删除。");
   if((attr&FileAttributes.Directory)!=0)AssertPlain(entry);
  }
 }
 async Task Remove(){
  if(complete){Close();return;}
  bool delete=erase.Enabled&&erase.Checked;
  if(delete&&MessageBox.Show(this,"确定永久删除以下目录中的全部码表、设置、词条和备份？\n\n"+data,"确认删除当前用户数据",MessageBoxButtons.YesNo,MessageBoxIcon.Warning,MessageBoxDefaultButton.Button2)!=DialogResult.Yes)return;
  busy=true;uninstall.Enabled=false;erase.Enabled=false;status.Text="正在卸载，请在 Windows 授权窗口中确认。";
  try{
   if(delete)AssertPlain(data);
   // Keep this ordinary-user process alive across UAC; no user path is sent to the administrator.
   int code=await Task.Run(()=>{var p=Process.Start(new ProcessStartInfo(Path.Combine(root,"unins000.exe"),"/SILENT /NORESTART /RESTARTEXITCODE=3010"){UseShellExecute=true,Verb="runas"});p.WaitForExit();return p.ExitCode;});
   if(code!=0&&code!=3010)throw new IOException("卸载未完成或已取消，用户数据未删除。退出码："+code);
   if(delete){AssertPlain(data);if(Directory.Exists(data))Directory.Delete(data,true);}
   status.Text="卸载完成。"+(delete?"当前用户数据已删除。":"用户数据保留在：\n"+data)+(code==3010?"\n部分程序文件被占用，请重启电脑完成清理。":"\n请重开正在使用的应用。");
   complete=true;uninstall.Text="关闭";uninstall.Enabled=true;
  }catch(Exception ex){status.Text=ex.Message+"\n如仍有数据文件，保留位置："+data;uninstall.Enabled=true;erase.Enabled=!Elevated();}
  finally{busy=false;}
 }
}
