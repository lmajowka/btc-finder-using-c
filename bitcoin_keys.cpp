#include <node.h>
#include <node_buffer.h>
#include <v8.h>
#include <secp256k1.h>
#include <openssl/sha.h>
#include <openssl/ripemd.h>
#include <vector>
#include <string>
#include <iostream>
#include <sstream>
#include <iomanip>

// Base58 encoding characters
const char* BASE58_ALPHABET = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

namespace demo {

using v8::Exception;
using v8::FunctionCallbackInfo;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::String;
using v8::Value;

// Helper function to convert bytes to hex string for debugging
std::string BytesToHex(const unsigned char* data, size_t length) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < length; ++i) {
        ss << std::setw(2) << static_cast<int>(data[i]);
    }
    return ss.str();
}

std::string Base58Encode(const std::vector<unsigned char>& input) {
    // Count leading zeros
    int zeroes = 0;
    while (zeroes < input.size() && input[zeroes] == 0) {
        zeroes++;
    }

    // Allocate enough space in big-endian base58 representation.
    std::vector<unsigned char> b58((input.size() - zeroes) * 138 / 100 + 1);

    // Process the bytes.
    for (size_t i = zeroes; i < input.size(); ++i) {
        int carry = input[i];
        for (auto it = b58.rbegin(); it != b58.rend(); ++it) {
            carry += 256 * (*it);
            *it = carry % 58;
            carry /= 58;
        }
    }

    // Skip leading zeros in base58 result.
    auto it = b58.begin();
    while (it != b58.end() && *it == 0) {
        ++it;
    }

    // Translate the result into a string.
    std::string result;
    result.reserve(zeroes + (b58.end() - it));
    result.assign(zeroes, '1');
    while (it != b58.end()) {
        result += BASE58_ALPHABET[*(it++)];
    }
    return result;
}

std::string GenerateBitcoinAddress(const unsigned char* publicKey) {
    unsigned char sha256Hash[SHA256_DIGEST_LENGTH];
    SHA256(publicKey, 33, sha256Hash);

    // std::cout << "SHA256 Hash: " << BytesToHex(sha256Hash, SHA256_DIGEST_LENGTH) << std::endl;

    unsigned char ripemd160Hash[RIPEMD160_DIGEST_LENGTH];
    RIPEMD160(sha256Hash, SHA256_DIGEST_LENGTH, ripemd160Hash);

    // std::cout << "RIPEMD160 Hash: " << BytesToHex(ripemd160Hash, RIPEMD160_DIGEST_LENGTH) << std::endl;

    std::vector<unsigned char> address;
    address.push_back(0x00); // Version byte for mainnet
    address.insert(address.end(), ripemd160Hash, ripemd160Hash + RIPEMD160_DIGEST_LENGTH);

    unsigned char checksum[SHA256_DIGEST_LENGTH];
    SHA256(address.data(), address.size(), sha256Hash);
    SHA256(sha256Hash, SHA256_DIGEST_LENGTH, checksum);

    // std::cout << "Checksum: " << BytesToHex(checksum, 4) << std::endl;

    address.insert(address.end(), checksum, checksum + 4); // Add first 4 bytes of the checksum

    // std::cout << "Address before Base58: " << BytesToHex(address.data(), address.size()) << std::endl;

    return Base58Encode(address);
}

void GetBitcoinAddress(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();

    if (args.Length() < 1 || !args[0]->IsString()) {
        isolate->ThrowException(Exception::TypeError(
            String::NewFromUtf8(isolate, "String expected").ToLocalChecked()));
        return;
    }

    v8::String::Utf8Value privateKeyHex(isolate, args[0]);
    const char* privateKeyStr = *privateKeyHex;

    unsigned char privateKey[32];
    for (int i = 0; i < 32; ++i) {
        sscanf(&privateKeyStr[i * 2], "%2hhx", &privateKey[i]);
    }

    // std::cout << "Private Key: " << BytesToHex(privateKey, 32) << std::endl;

    secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN);
    secp256k1_pubkey pubkey;

    if (!secp256k1_ec_pubkey_create(ctx, &pubkey, privateKey)) {
        secp256k1_context_destroy(ctx);
        isolate->ThrowException(Exception::Error(
            String::NewFromUtf8(isolate, "Failed to create public key").ToLocalChecked()));
        return;
    }

    unsigned char output[33];
    size_t outputLen = 33;
    secp256k1_ec_pubkey_serialize(ctx, output, &outputLen, &pubkey, SECP256K1_EC_COMPRESSED);

    secp256k1_context_destroy(ctx);

    // std::cout << "Public Key: " << BytesToHex(output, 33) << std::endl;

    std::string bitcoinAddress = GenerateBitcoinAddress(output);

    // std::cout << "Bitcoin Address: " << bitcoinAddress << std::endl;

    args.GetReturnValue().Set(String::NewFromUtf8(isolate, bitcoinAddress.c_str()).ToLocalChecked());
}

void Initialize(Local<Object> exports) {
    NODE_SET_METHOD(exports, "getBitcoinAddress", GetBitcoinAddress);
}

NODE_MODULE(NODE_GYP_MODULE_NAME, Initialize)

}  // namespace demo
