using System;
using System.Drawing;
using System.Windows.Forms;
using System.Runtime.InteropServices;
using System.Text.Json;
using System.IO;
using System.Diagnostics;

namespace OverlayImageHint
{
    public class Settings
    {
        public string? image_name { get; set; } = "hint.png";
        public int image_transparency { get; set; } = 90;
        public Keys key_for_show { get; set; } = Keys.F8;
    }

    static class Program
    {
        [STAThread]
        static void Main()
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(new OverlayForm());
        }
    }

    public class OverlayForm : Form
    {
        private PictureBox pictureBox;
        private GlobalKeyboardHook keyboardHook;
        private bool isOverlayVisible = false;
        private Settings settings;
        private NotifyIcon trayIcon;

        // Импорт Win32 API для создания окна поверх всех других окон
        [DllImport("user32.dll", SetLastError = true)]
        private static extern int SetWindowLong(IntPtr hWnd, int nIndex, int dwNewLong);
        
        [DllImport("user32.dll", SetLastError = true)]
        private static extern int GetWindowLong(IntPtr hWnd, int nIndex);
        
        private const int GWL_EXSTYLE = -20;
        private const int WS_EX_TRANSPARENT = 0x00000020;
        private const int WS_EX_LAYERED = 0x00080000;
        private const int WS_EX_TOPMOST = 0x00000008;

        public OverlayForm()
        {
            // Загружаем настройки
            LoadSettings();

            // Настройка основной формы
            this.FormBorderStyle = FormBorderStyle.None;
            this.ShowInTaskbar = false;
            this.TopMost = true;
            this.WindowState = FormWindowState.Maximized;
            this.BackColor = Color.Black;
            this.TransparencyKey = Color.Black;
            this.Opacity = settings.image_transparency / 100.0;

            // Создание PictureBox для отображения изображения
            pictureBox = new PictureBox
            {
                Dock = DockStyle.Fill,
                SizeMode = PictureBoxSizeMode.Zoom,
                BackColor = Color.Transparent
            };

            try
            {
                // Загрузка изображения из файла
                pictureBox.Image = Image.FromFile(settings.image_name);
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Ошибка загрузки изображения: {ex.Message}", "Ошибка", 
                    MessageBoxButtons.OK, MessageBoxIcon.Error);
                Application.Exit();
            }

            this.Controls.Add(pictureBox);

            // Настройка значка в трее
            SetupTrayIcon();

            // Инициализация и настройка глобального перехватчика клавиатуры
            keyboardHook = new GlobalKeyboardHook();
            keyboardHook.KeyDown += KeyboardHook_KeyDown;
            keyboardHook.KeyUp += KeyboardHook_KeyUp;
            keyboardHook.Hook();

            // Начальное состояние - скрыто
            this.Visible = false;
            
            // Делаем форму кликопрозрачной
            SetClickThrough();
        }

        private void LoadSettings()
        {
            string settingsPath = "settings.json";
            
            // Создаем настройки по умолчанию
            settings = new Settings();
            
            // Если файл настроек существует, загружаем его
            if (File.Exists(settingsPath))
            {
                try
                {
                    string jsonString = File.ReadAllText(settingsPath);
                    settings = JsonSerializer.Deserialize<Settings>(jsonString) ?? new Settings();
                }
                catch (Exception ex)
                {
                    MessageBox.Show($"Ошибка загрузки настроек: {ex.Message}\nБудут использованы настройки по умолчанию.", 
                        "Ошибка", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                }
            }
            else
            {
                // Создаем файл с настройками по умолчанию
                SaveSettings();
            }
        }

        private void SaveSettings()
        {
            try
            {
                var options = new JsonSerializerOptions { WriteIndented = true };
                string jsonString = JsonSerializer.Serialize(settings, options);
                File.WriteAllText("settings.json", jsonString);
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Ошибка сохранения настроек: {ex.Message}", "Ошибка", 
                    MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void SetupTrayIcon()
        {
            // Создаем меню для иконки в трее
            ContextMenuStrip trayMenu = new ContextMenuStrip();
            
            // Добавляем пункт "Открыть GitHub"
            ToolStripMenuItem openGitHubItem = new ToolStripMenuItem("Открыть GitHub");
            openGitHubItem.Click += (sender, e) => 
            {
                Process.Start(new ProcessStartInfo
                {
                    FileName = "https://github.com/nntdgrss/OverlayImageHint",
                    UseShellExecute = true
                });
            };
            trayMenu.Items.Add(openGitHubItem);
            
            // Добавляем разделитель
            trayMenu.Items.Add(new ToolStripSeparator());
            
            // Добавляем пункт "Завершить работу"
            ToolStripMenuItem exitItem = new ToolStripMenuItem("Завершить работу");
            exitItem.Click += (sender, e) => Application.Exit();
            trayMenu.Items.Add(exitItem);
            
            // Создаем иконку в трее
            trayIcon = new NotifyIcon
            {
                Text = "Оверлей подсказки",
                Visible = true,
                ContextMenuStrip = trayMenu
            };
            
            // Пытаемся загрузить иконку из ресурсов или файла
            try
            {
                // Проверяем наличие иконки в ресурсах
                if (File.Exists("icon.ico"))
                {
                    trayIcon.Icon = new Icon("icon.ico");
                }
                else
                {
                    // Используем стандартную иконку
                    trayIcon.Icon = SystemIcons.Application;
                }
            }
            catch
            {
                // В случае ошибки используем стандартную иконку
                trayIcon.Icon = SystemIcons.Application;
            }
        }

        private void SetClickThrough()
        {
            // Делаем окно прозрачным для кликов
            int exStyle = GetWindowLong(this.Handle, GWL_EXSTYLE);
            exStyle |= WS_EX_LAYERED | WS_EX_TRANSPARENT;
            SetWindowLong(this.Handle, GWL_EXSTYLE, exStyle);
        }

        private void KeyboardHook_KeyDown(object? sender, KeyEventArgs e)
        {
            if (e.KeyCode == settings.key_for_show && !isOverlayVisible)
            {
                this.Visible = true;
                isOverlayVisible = true;
            }
        }

        private void KeyboardHook_KeyUp(object? sender, KeyEventArgs e)
        {
            if (e.KeyCode == settings.key_for_show && isOverlayVisible)
            {
                this.Visible = false;
                isOverlayVisible = false;
            }
        }

        protected override void OnFormClosed(FormClosedEventArgs e)
        {
            keyboardHook.Unhook();
            trayIcon.Dispose();
            base.OnFormClosed(e);
        }
    }

    public class GlobalKeyboardHook
    {
        // Делегаты для обработки событий клавиатуры
        public delegate int KeyboardHookProc(int code, int wParam, ref KeyboardHookStruct lParam);

        // Структура для хранения данных о клавиатурных событиях
        public struct KeyboardHookStruct
        {
            public int vkCode;
            public int scanCode;
            public int flags;
            public int time;
            public int dwExtraInfo;
        }

        // Константы для хука клавиатуры
        private const int WH_KEYBOARD_LL = 13;
        private const int WM_KEYDOWN = 0x0100;
        private const int WM_KEYUP = 0x0101;
        private const int WM_SYSKEYDOWN = 0x0104;
        private const int WM_SYSKEYUP = 0x0105;

        // Импорт необходимых функций Win32 API
        [DllImport("user32.dll", CharSet = CharSet.Auto, SetLastError = true)]
        private static extern IntPtr SetWindowsHookEx(int idHook, KeyboardHookProc lpfn, IntPtr hMod, uint dwThreadId);

        [DllImport("user32.dll", CharSet = CharSet.Auto, SetLastError = true)]
        private static extern bool UnhookWindowsHookEx(IntPtr hhk);

        [DllImport("user32.dll", CharSet = CharSet.Auto, SetLastError = true)]
        private static extern int CallNextHookEx(IntPtr hhk, int nCode, int wParam, ref KeyboardHookStruct lParam);

        [DllImport("kernel32.dll", CharSet = CharSet.Auto, SetLastError = true)]
        private static extern IntPtr GetModuleHandle(string? lpModuleName);

        // События клавиатуры
        public event KeyEventHandler? KeyDown;
        public event KeyEventHandler? KeyUp;

        // Хук и процедура обработки
        private IntPtr hookId = IntPtr.Zero;
        private KeyboardHookProc hookProc;

        public GlobalKeyboardHook()
        {
            hookProc = HookCallback;
        }

        public void Hook()
        {
            IntPtr hInstance = GetModuleHandle(null);
            hookId = SetWindowsHookEx(WH_KEYBOARD_LL, hookProc, hInstance, 0);
        }

        public void Unhook()
        {
            if (hookId != IntPtr.Zero)
            {
                UnhookWindowsHookEx(hookId);
                hookId = IntPtr.Zero;
            }
        }

        private int HookCallback(int code, int wParam, ref KeyboardHookStruct lParam)
        {
            if (code >= 0)
            {
                Keys key = (Keys)lParam.vkCode;
                KeyEventArgs e = new KeyEventArgs(key);

                if ((wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) && KeyDown != null)
                {
                    KeyDown(this, e);
                }
                else if ((wParam == WM_KEYUP || wParam == WM_SYSKEYUP) && KeyUp != null)
                {
                    KeyUp(this, e);
                }

                if (e.Handled)
                    return 1;
            }

            return CallNextHookEx(hookId, code, wParam, ref lParam);
        }
    }
} 