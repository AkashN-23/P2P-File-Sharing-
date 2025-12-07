//Diffe_Hellman Key Exchange
#ifndef DH_HELPER_H
#define DH_HELPER_H

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/dh.h>
#include <openssl/err.h>
#include <vector>
#include <iostream>
#include <cstring>

class DiffieHellman {
    EVP_PKEY *dh_params = nullptr;
    EVP_PKEY *dh_keypair = nullptr;
    EVP_PKEY_CTX *kctx = nullptr;

public:
    DiffieHellman() {}

    ~DiffieHellman() {
        if (dh_params) EVP_PKEY_free(dh_params);
        if (dh_keypair) EVP_PKEY_free(dh_keypair);
        if (kctx) EVP_PKEY_CTX_free(kctx);
    }

    // 1. Initialize DH Parameters (Standard 2048-bit prime)
    bool init() {
        // Use standard RFC 3526 parameters (safe and standard)
        dh_params = EVP_PKEY_new();
        DH* dh = DH_get_2048_256(); // Standard 2048-bit prime
        if (!dh || !EVP_PKEY_assign_DH(dh_params, dh)) {
            std::cerr << "[-] Failed to generate DH params" << std::endl;
            return false;
        }
        return true;
    }

    // 2. Generate My Public/Private Key Pair
    bool generate_keys() {
        kctx = EVP_PKEY_CTX_new(dh_params, NULL);
        if (!kctx || EVP_PKEY_keygen_init(kctx) <= 0) return false;
        if (EVP_PKEY_keygen(kctx, &dh_keypair) <= 0) return false;
        return true;
    }

    // 3. Extract My Public Key to send to the other party
    std::vector<unsigned char> get_public_key() {
        unsigned char *pub_key_bio = nullptr;
        int len = i2d_PUBKEY(dh_keypair, &pub_key_bio);
        if (len <= 0) return {};
        
        std::vector<unsigned char> output(pub_key_bio, pub_key_bio + len);
        OPENSSL_free(pub_key_bio);
        return output;
    }

    // 4. Compute Shared Secret using Peer's Public Key
    std::vector<unsigned char> compute_shared_secret(const std::vector<unsigned char>& peer_pub_key_data) {
        // Convert raw bytes back to OpenSSL Key structure
        const unsigned char* p = peer_pub_key_data.data();
        EVP_PKEY *peer_key = d2i_PUBKEY(NULL, &p, peer_pub_key_data.size());
        if (!peer_key) {
            std::cerr << "[-] Invalid peer key format" << std::endl;
            return {};
        }

        // Derive the secret
        EVP_PKEY_CTX *derive_ctx = EVP_PKEY_CTX_new(dh_keypair, NULL);
        if (!derive_ctx || EVP_PKEY_derive_init(derive_ctx) <= 0) return {};
        if (EVP_PKEY_derive_set_peer(derive_ctx, peer_key) <= 0) return {};

        size_t secret_len;
        if (EVP_PKEY_derive(derive_ctx, NULL, &secret_len) <= 0) return {};

        std::vector<unsigned char> secret(secret_len);
        if (EVP_PKEY_derive(derive_ctx, secret.data(), &secret_len) <= 0) return {};

        EVP_PKEY_free(peer_key);
        EVP_PKEY_CTX_free(derive_ctx);
        
        // IMPORTANT: The secret is usually long. We hash it to get exactly 32 bytes for AES-256.
        // We use SHA-256 for this.
        unsigned char hash[32];
        EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
        EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL);
        EVP_DigestUpdate(mdctx, secret.data(), secret_len);
        EVP_DigestFinal_ex(mdctx, hash, NULL);
        EVP_MD_CTX_free(mdctx);

        return std::vector<unsigned char>(hash, hash + 32);
    }
};

#endif