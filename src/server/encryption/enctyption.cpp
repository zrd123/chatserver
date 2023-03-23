#include "encryption.hpp"
#include <openssl/aes.h>
#include <openssl/pem.h>
#include <openssl/evp.h>
#include <iostream>

Encryption *Encryption::encryption = nullptr;
std::string Encryption::AES = "1234567887654321";

Encryption::Encryption() :
    gen(rd()), dist(0, 255)
{
    iv = reinterpret_cast<unsigned char *>(malloc(16));
    // 生成 16 字节的随机向量
    for (int i = 0; i < 16; ++i) {
        iv[i] = static_cast<unsigned char>(dist(gen));
    }
}

Encryption::~Encryption()
{
    if(iv){
        free(iv);
        iv = nullptr;
    }
    if(encryption){
        delete encryption;
        encryption = nullptr;
    }
}

Encryption *Encryption::getEncryption()
{
    if (nullptr == encryption) {
          encryption = new Encryption();
        }
    return encryption;
}

uint32_t Encryption::doEncryption(std::string &data, int datalen, unsigned char*iv)
{
    int outlen = 0;
    int encLen = 0;
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    EVP_CIPHER_CTX_init(ctx);
//    unsigned char encData[1024];
    unsigned char *encData = (unsigned char*)malloc(datalen);
    EVP_CipherInit_ex(ctx, AES_128_CBC(), NULL, (unsigned char*)AES.c_str(), iv, 1);
    EVP_CIPHER_CTX_set_padding(ctx,  0);
    EVP_CipherUpdate(ctx, encData, &outlen, (unsigned char*)data.c_str(), datalen);
    encLen = outlen;
    EVP_CipherFinal(ctx, encData+outlen, &outlen);
    encLen += outlen;
    std::cout << encLen << " " << outlen << " :" << *encData << std::endl;;
    data = std::string((char*)encData, encLen);
//    data[encLen] = '\0';
    std::cout << "size:" <<data.size() << std::endl;
    EVP_CIPHER_CTX_free(ctx);
    free(encData);
    return (uint32_t)encLen;
}

uint32_t Encryption::doDecryption(std::string &data, int datalen, unsigned char* iv)
{
    int decLen = 0;
    int outlen = 0;
    unsigned char *decData = (unsigned char*)malloc(datalen);
//    unsigned char decData[1024];
    EVP_CIPHER_CTX *ctx2;
    ctx2 = EVP_CIPHER_CTX_new();
    EVP_CipherInit_ex(ctx2, AES_128_CBC(), NULL, (unsigned char*)AES.c_str(), iv, 0);
    EVP_CIPHER_CTX_set_padding(ctx2, 0);
    EVP_CipherUpdate(ctx2, decData, &outlen, (unsigned char*)data.c_str(), datalen);
    decLen = outlen;
    EVP_CipherFinal(ctx2, decData+outlen, &outlen);
    decLen += outlen;
    EVP_CIPHER_CTX_free(ctx2);
//    std::cout << std::addressof(data.data_ptr()) << std::endl;
    data = "";
    data = std::string((char*)decData, decLen);
//    std::cout << std::addressof(data.data_ptr()) << std::endl;
    free(decData);
    return (uint32_t)decLen;
}

void Encryption::randomIV(unsigned char *out)
{
    for (int i = 0; i < 16; ++i) {
        out[i] = iv[i] = static_cast<unsigned char>(dist(gen));
    }
}


bool Encryption::MakeRsaKeySSL(const char *savePrivateKeyFilePath, const  char *savePublicKeyFilePath) {
    int             ret = 0;
    RSA             *r = NULL;
    BIGNUM          *bne = NULL;
    BIO             *bp_public = NULL, *bp_private = NULL;

    int             bits = 2048;
    unsigned long   e = RSA_F4;

    // 1. generate rsa key
    bne = BN_new();
    ret = BN_set_word(bne, e);
    if (ret != 1) {
        fprintf(stderr, "MakeLocalKeySSL BN_set_word err \n");
        goto free_all;
    }

    r = RSA_new();
    ret = RSA_generate_key_ex(r, bits, bne, NULL);
    if (ret != 1) {
        fprintf(stderr, "MakeLocalKeySSL RSA_generate_key_ex err \n");
        goto free_all;
    }

    // 2. save public key
    if (savePublicKeyFilePath != NULL) {
        bp_public = BIO_new_file(savePublicKeyFilePath, "w+");
        ret = PEM_write_bio_RSAPublicKey(bp_public, r);
        if (ret != 1) {
            fprintf(stderr, "MakeLocalKeySSL PEM_write_bio_RSAPublicKey err \n");
            goto free_all;
        }
    }

    // 3. save private key
    if (savePrivateKeyFilePath != NULL) {
        bp_private = BIO_new_file(savePrivateKeyFilePath, "w+");
        ret = PEM_write_bio_RSAPrivateKey(bp_private, r, NULL, NULL, 0, NULL, NULL);
    }

    // 4. free
free_all:

    BIO_free_all(bp_public);
    BIO_free_all(bp_private);
    RSA_free(r);
    BN_free(bne);

    return (ret == 1);
}

void Encryption::generateAES(const std::string &pubkey)
{
    // 准备数据
       std::string data = "我是峰子疯子疯子疯子疯子疯子疯子我是峰子疯子疯子疯子疯子疯子疯子";

}
