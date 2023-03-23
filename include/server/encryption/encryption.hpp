#ifndef ENCRYPTION_H
#define ENCRYPTION_H

#include <openssl/err.h>
#include <openssl/des.h>
#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
// #include <openssl/md5.h>
// #include <openssl/sha.h>
#include <string> 
#include <vector>
#include <unordered_map>
#include <iostream>

class Encrypto{
public:
    Encrypto(){
        BIO* in1 = BIO_new(BIO_s_file());
	    BIO* in2 = BIO_new(BIO_s_file());
        if (in1 == NULL) {
            std::cout << "BIO_new failed" << std::endl;
        }
        std::string path1("pubkey.pem");
        std::string path2("prikey.pem");
        BIO *pub = BIO_new(BIO_s_mem());
        BIO *pri = BIO_new(BIO_s_mem());
        BIO_read_filename(in1, path1.c_str());
        BIO_read_filename(in2, path2.c_str());
        RSA* rsa = PEM_read_bio_RSAPublicKey(in1, NULL, NULL, NULL);
        RSA* rsa1 = PEM_read_bio_RSAPrivateKey(in2, NULL, NULL, NULL);
        if (rsa == NULL) {
            std::cout << "PEM_read_bio_RSA_PUBKEY failed" << std::endl;
        }
        PEM_write_bio_RSA_PUBKEY(pub, rsa);	
        PEM_write_bio_RSAPrivateKey(pri, rsa1, NULL, NULL, 0, NULL, NULL);
        size_t len1= BIO_pending(pub); 
        size_t len2= BIO_pending(pri); 
        std::cout << len1<< std::endl;
        std::cout << len2<< std::endl;
        char *k1 = (char*)malloc(len1+1);
        char *k2 = (char*)malloc(len2+1);
        pubKey.resize(len1+1);
        priKey.resize(len2+1);
        BIO_read(pub, k1, len1);
        BIO_read(pri, k2, len2);
        k1[len1]='\0';
        k2[len2]='\0';
        pubKey = k1;
        priKey = k2;
        std::cout << pubKey << std::endl;
        std::cout << priKey << std::endl;
        BIO_free(in1);
        BIO_free(in2);
        BIO_free(pub);
        BIO_free(pri);
        //auto it = EncryptByPubkeyString(srcText, pubkey);
        //auto ret = DecryptByPrikeyString(const_cast<char*>(it.c_str()), it.size(), prikey);
    }

    // 通过私钥字符串进行解密
    std::string DecryptByPrikeyString(char* cipher, uint32_t len,
                                    const std::string prikey) {
        BIO* in = BIO_new_mem_buf((void*)prikey.c_str(), -1);
        if (in == NULL) {
            std::cout << "BIO_new_mem_buf failed" << std::endl;
            return "";
        }

        RSA* rsa = PEM_read_bio_RSAPrivateKey(in, NULL, NULL, NULL);
        BIO_free(in);
        if (rsa == NULL) {
            std::cout << "PEM_read_bio_RSAPrivateKey failed" << std::endl;
            return "";
        }

        int size = RSA_size(rsa);
        std::string data;
        data.resize(size);
        int ret = RSA_private_decrypt(len, (unsigned char*)cipher,
                                    (unsigned char*)data.data(), rsa,
                                    RSA_PKCS1_PADDING);
        RSA_free(rsa);
        if (ret == -1) {
            std::cout << "RSA_private_decrypt failed" << std::endl;
            return "";
        }
        return data;
}
    void doEncrypto();

private:
    std::string priKey;
    std::string pubKey;
    std::unordered_map<uint32_t, std::string> key;
};


#include <random>

class Encryption
{
public:
    ~Encryption();

    static Encryption *getEncryption();
    static uint32_t doEncryption(std::string &str, int datalen, unsigned char*);
    static uint32_t doDecryption(std::string &str, int datalen, unsigned char*);
    void randomIV(unsigned char *);

private:
    static std::string AES;
    static Encryption *encryption;
    unsigned char *iv;
    std::random_device rd;
    std::mt19937 gen;
    std::uniform_int_distribution<int> dist;

#define OPENSSL_NO_DEPRECATED
// 密钥长度
#define KEY_LENGTH  2048             
// 公钥路径
#define PUB_KEY_FILE "pubkey.pem"    
// 私钥路径
#define PRI_KEY_FILE "prikey.pem"   
//加密模式
#define AES_128_CBC EVP_aes_128_cbc

    Encryption();
    bool MakeRsaKeySSL(const char *savePrivateKeyFilePath, const  char *savePublicKeyFilePath);
    void generateAES(const std::string &pubkey);
};

#endif // ENCRYPTION_H
