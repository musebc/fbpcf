/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <filesystem>
#include <fstream>
#include <ostream>
#include <random>

namespace fbpcf::engine::communication {

// creates a cert file, key file, and passphrase file
// in the provided directory
// it returns a sub-directory where the certs are stored.
// this is so that when multiple runs are happening in
// parallel, we don't overwrite certs that other tests generate
inline std::string setUpTlsFiles() {
  std::random_device rd;
  std::mt19937_64 e(rd());
  std::uniform_int_distribution<int64_t> dist(0, 1000000);
  auto randomInt = dist(e);
  std::string tmpDir = std::filesystem::temp_directory_path();
  std::string dirToCreate = tmpDir + "/" + std::to_string(randomInt);

  mkdir(dirToCreate.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

  /*
   * Create Root CA cert
   */
  RSA* rsa_for_ca;
  rsa_for_ca = RSA_generate_key(
      4096, /* number of bits for the key */
      RSA_F4, /* exponent */
      nullptr, /* callback */
      nullptr /* callback argument */
  );

  EVP_PKEY* pkey_for_ca;
  pkey_for_ca = EVP_PKEY_new();

  EVP_PKEY_assign_RSA(pkey_for_ca, rsa_for_ca);

  X509* x509_for_ca;
  x509_for_ca = X509_new();

  X509_gmtime_adj(X509_get_notBefore(x509_for_ca), 0);
  X509_gmtime_adj(
      X509_get_notAfter(x509_for_ca), 31536000L); // 1 year in seconds

  X509_set_pubkey(x509_for_ca, pkey_for_ca);

  X509_NAME* name_for_ca;
  name_for_ca = X509_get_subject_name(x509_for_ca);

  // These are metadata about the host, like country,
  // company name, etc. They are required, but not important
  // because these are just self signed for testing purposes.
  // We can leave them as empty strings.
  X509_NAME_add_entry_by_txt(
      name_for_ca, "C", MBSTRING_ASC, (unsigned char*)"", -1, -1, 0);
  X509_NAME_add_entry_by_txt(
      name_for_ca, "O", MBSTRING_ASC, (unsigned char*)"root org", -1, -1, 0);
  X509_NAME_add_entry_by_txt(
      name_for_ca, "CN", MBSTRING_ASC, (unsigned char*)"test ca", -1, -1, 0);
  X509_set_issuer_name(x509_for_ca, name_for_ca);
  X509_sign(x509_for_ca, pkey_for_ca, EVP_sha1());

  /*
   * Create server cert signed using root CA
   */
  RSA* rsa_for_server;
  rsa_for_server = RSA_generate_key(
      4096, /* number of bits for the key */
      RSA_F4, /* exponent */
      nullptr, /* callback */
      nullptr /* callback argument */
  );

  EVP_PKEY* pkey_for_server;
  pkey_for_server = EVP_PKEY_new();

  EVP_PKEY_assign_RSA(pkey_for_server, rsa_for_server);

  // write cert to file
  FILE* cafile;
  cafile = fopen((dirToCreate + "/ca_cert.pem").c_str(), "wb");
  PEM_write_X509(cafile, x509_for_ca);
  fclose(cafile);

  X509* x509_for_server;
  x509_for_server = X509_new();

  // expiry date
  X509_gmtime_adj(X509_get_notBefore(x509_for_server), 0);
  X509_gmtime_adj(
      X509_get_notAfter(x509_for_server), 31536000L); // 1 year in seconds

  X509_set_pubkey(x509_for_server, pkey_for_server);

  X509_NAME* name_for_server;
  name_for_server = X509_get_subject_name(x509_for_server);

  // These are metadata about the host, like country,
  // company name, etc. They are required, but not important
  // because these are just self signed for testing purposes.
  // We can leave them as empty strings.
  X509_NAME_add_entry_by_txt(
      name_for_server, "C", MBSTRING_ASC, (unsigned char*)"", -1, -1, 0);
  X509_NAME_add_entry_by_txt(
      name_for_server, "O", MBSTRING_ASC, (unsigned char*)"", -1, -1, 0);
  X509_NAME_add_entry_by_txt(
      name_for_server, "CN", MBSTRING_ASC, (unsigned char*)"", -1, -1, 0);
  X509_set_issuer_name(x509_for_server, name_for_ca);
  X509_sign(x509_for_server, pkey_for_ca, EVP_sha1());

  // write private key to file
  FILE* keyfile;
  keyfile = fopen((dirToCreate + "/key.pem").c_str(), "wb");
  PEM_write_PrivateKey(
      keyfile,
      pkey_for_server,
      EVP_des_ede3_cbc(), /* cipher */
      (unsigned char*)"test_passphrase", /* for tests only */
      15, /* length of the passphrase string */
      nullptr, /* callback */
      nullptr /* callback argument */
  );
  fclose(keyfile);

  // write cert to file
  FILE* certfile;
  certfile = fopen((dirToCreate + "/cert.pem").c_str(), "wb");
  PEM_write_X509(certfile, x509_for_server);
  fclose(certfile);

  // write passphrase to file
  std::ofstream passfile(dirToCreate + "/passphrase.pem");
  passfile << "test_passphrase";
  passfile.close();

  return dirToCreate;
}

// deletes TLS files generated by the function above
inline void deleteTlsFiles(std::string dir) {
  std::filesystem::remove_all(dir);
}

} // namespace fbpcf::engine::communication
