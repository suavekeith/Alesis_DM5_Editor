const { app, BrowserWindow, ipcMain, dialog, Menu, shell } = require('electron');
const path = require('path');
const fs   = require('fs');

function createWindow() {
  const win = new BrowserWindow({
    width: 1280,
    height: 960,
    minWidth: 900,
    minHeight: 700,
    title: 'DM5 Editor',
    webPreferences: {
      nodeIntegration:  false,
      contextIsolation: true,
      preload: path.join(__dirname, 'preload.js')
    }
  });

  win.loadFile('DM5Editor.html');

  // Auto-grant MIDI + sysex permission — no browser popup needed
  win.webContents.session.setPermissionRequestHandler((_wc, permission, callback) => {
    callback(permission === 'midi' || permission === 'midiSysex');
  });

  // Native menu
  const menu = Menu.buildFromTemplate([
    {
      label: 'File',
      submenu: [
        { label: 'Export Kits…', accelerator: 'CmdOrCtrl+S',
          click: () => win.webContents.send('menu-export') },
        { label: 'Import Kits…', accelerator: 'CmdOrCtrl+O',
          click: () => win.webContents.send('menu-import') },
        { type: 'separator' },
        { role: 'quit' }
      ]
    },
    { role: 'editMenu' },
    { role: 'viewMenu' },
    {
      label: 'Help',
      submenu: [
        { label: 'DM5 Sound Chart',
          click: () => shell.openExternal('https://www.alesis.com/rscdn/1580/documents/dm5_soundchart.pdf') }
      ]
    }
  ]);
  Menu.setApplicationMenu(menu);
}

app.whenReady().then(createWindow);

app.on('window-all-closed', () => { if (process.platform !== 'darwin') app.quit(); });
app.on('activate', () => { if (BrowserWindow.getAllWindows().length === 0) createWindow(); });

// ── IPC: save file ──────────────────────────────────────────
ipcMain.handle('save-file', async (_e, { defaultName, content }) => {
  const { filePath } = await dialog.showSaveDialog({
    defaultPath: defaultName,
    filters: [{ name: 'JSON', extensions: ['json'] }]
  });
  if (!filePath) return { success: false };
  fs.writeFileSync(filePath, content, 'utf8');
  return { success: true, path: filePath };
});

// ── IPC: open file ──────────────────────────────────────────
ipcMain.handle('open-file', async () => {
  const { filePaths } = await dialog.showOpenDialog({
    filters: [{ name: 'JSON', extensions: ['json'] }],
    properties: ['openFile']
  });
  if (!filePaths || !filePaths[0]) return { success: false };
  const content = fs.readFileSync(filePaths[0], 'utf8');
  return { success: true, content };
});
