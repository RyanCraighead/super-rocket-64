"""AES ECB for the hash-pinned local asset container, using Windows CNG.

No keys, game data, networking or installation. Other platforms use the optional
cryptography package, as the original exporter did.
"""
import os


def aes_ecb(data, key, encrypt=False):
    if not data or len(data) % 16 or len(key) not in (16, 24, 32):
        raise ValueError('AES requires whole blocks and a valid key length')
    if os.name != 'nt':
        from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
        cipher = Cipher(algorithms.AES(key), modes.ECB())
        operation = cipher.encryptor() if encrypt else cipher.decryptor()
        return operation.update(data) + operation.finalize()
    import ctypes as c
    from ctypes import wintypes as w
    api = c.WinDLL('bcrypt.dll')
    handle = c.c_void_p
    api.BCryptOpenAlgorithmProvider.argtypes = [c.POINTER(handle), w.LPCWSTR, w.LPCWSTR, w.ULONG]
    api.BCryptSetProperty.argtypes = [handle, w.LPCWSTR, c.c_void_p, w.ULONG, w.ULONG]
    api.BCryptGenerateSymmetricKey.argtypes = [handle, c.POINTER(handle), c.c_void_p, w.ULONG, c.c_void_p, w.ULONG, w.ULONG]
    signature = [handle, c.c_void_p, w.ULONG, c.c_void_p, c.c_void_p, w.ULONG, c.c_void_p, w.ULONG, c.POINTER(w.ULONG), w.ULONG]
    api.BCryptEncrypt.argtypes = api.BCryptDecrypt.argtypes = signature
    api.BCryptDestroyKey.argtypes = [handle]
    api.BCryptCloseAlgorithmProvider.argtypes = [handle, w.ULONG]
    def check(status):
        if status != 0:
            raise OSError('Windows AES operation failed: 0x%08x' % (status & 0xffffffff))
    algorithm, symmetric = handle(), handle()
    try:
        check(api.BCryptOpenAlgorithmProvider(c.byref(algorithm), 'AES', None, 0))
        mode = c.create_unicode_buffer('ChainingModeECB')
        check(api.BCryptSetProperty(algorithm, 'ChainingMode', mode, c.sizeof(mode), 0))
        secret = c.create_string_buffer(key)
        check(api.BCryptGenerateSymmetricKey(algorithm, c.byref(symmetric), None, 0, secret, len(key), 0))
        source, target, written = c.create_string_buffer(data), c.create_string_buffer(len(data)), w.ULONG()
        operation = api.BCryptEncrypt if encrypt else api.BCryptDecrypt
        check(operation(symmetric, source, len(data), None, None, 0, target, len(data), c.byref(written), 0))
        if written.value != len(data):
            raise OSError('Windows AES output length mismatch')
        return target.raw
    finally:
        if symmetric:
            api.BCryptDestroyKey(symmetric)
        if algorithm:
            api.BCryptCloseAlgorithmProvider(algorithm, 0)
