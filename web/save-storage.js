/* Journey-only namespace. Keyboard preferences belong to separate keys.
 * localStorage.setItem replaces one value atomically, synchronously. */
Module.paldoSaveStore = (() => {
  const key = 'paldoyadam.journey.autosave';
  const backup = key + '.backup';
  const failure = e => e && (e.name === 'QuotaExceededError' || e.name === 'NS_ERROR_DOM_QUOTA_REACHED') ? 5 : 4;
  return {
    read(isBackup, out, cap) {
      try {
        const hex = localStorage.getItem(isBackup ? backup : key);
        if (hex === null) return -1;
        if (hex.length % 2 || hex.length > cap * 2 || !/^[0-9a-f]+$/.test(hex)) return -2;
        for (let i = 0; i < hex.length / 2; i++) HEAPU8[out + i] = parseInt(hex.slice(i * 2, i * 2 + 2), 16);
        return hex.length / 2;
      } catch (e) { return -failure(e); }
    },
    write(data, size, preserve) {
      try {
        let hex = '';
        for (let i = 0; i < size; i++) hex += HEAPU8[data + i].toString(16).padStart(2, '0');
        if (preserve) localStorage.setItem(backup, localStorage.getItem(key));
        localStorage.setItem(key, hex);
        return 0;
      } catch (e) { return failure(e); }
    }
  };
})();
