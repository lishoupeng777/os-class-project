const { contextBridge, ipcMain } = require('electron');

contextBridge.exposeInMainWorld('vfsAPI', {
  invokeCommand: (cmd, args) => ipcMain.invoke('vfs-command', { cmd, args })
});
