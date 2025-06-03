# This Python file uses the following encoding: utf-8

from cryptography.hazmat.primitives.asymmetric.x25519 import X25519PrivateKey
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
import sys


def generate_reference_value():
    priv_key1 = X25519PrivateKey.generate()
    pub_key1 = priv_key1.public_key()
    priv_key2 = X25519PrivateKey.generate()
    pub_key2 = priv_key2.public_key()
    assert len(priv_key1.private_bytes_raw()) == 32
    assert len(pub_key1.public_bytes_raw()) == 32

    shared_secret1 = priv_key1.exchange(pub_key2)
    shared_secret2 = priv_key2.exchange(pub_key1)
    assert shared_secret1 == shared_secret2
    assert len(shared_secret1) == 32


    edsign_key = Ed25519PrivateKey.generate()
    edsign_pub = edsign_key.public_key()
    pub_key1_sig = edsign_key.sign(pub_key1.public_bytes_raw())
    pub_key2_sig = edsign_key.sign(pub_key2.public_bytes_raw())


    return (priv_key1.private_bytes_raw() + pub_key1.public_bytes_raw()
            + priv_key2.private_bytes_raw() + pub_key2.public_bytes_raw()
            + shared_secret1
            + edsign_key.private_bytes_raw() + edsign_pub.public_bytes_raw()
            + pub_key1_sig + pub_key2_sig)


if __name__ == "__main__":
    with open(sys.argv[1], "wb") as f:
        for i in range(16):
            f.write(generate_reference_value())






