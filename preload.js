const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('electronAPI', {
  saveFile: (opts)  => ipcRenderer.invoke('save-file', opts),
  openFile: ()      => ipcRenderer.invoke('open-file'),
  onMenuExport: (cb) => ipcRenderer.on('menu-export', cb),
  onMenuImport: (cb) => ipcRenderer.on('menu-import', cb)
});
