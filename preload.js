const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('electronAPI', {
  saveFile: (opts)  => ipcRenderer.invoke('save-file', opts),
  openFile: ()      => ipcRenderer.invoke('open-file'),
  loadState: ()     => ipcRenderer.invoke('load-state'),
  saveState: (state) => ipcRenderer.invoke('save-state', state),
  onMenuExport: (cb) => ipcRenderer.on('menu-export', cb),
  onMenuImport: (cb) => ipcRenderer.on('menu-import', cb)
});
