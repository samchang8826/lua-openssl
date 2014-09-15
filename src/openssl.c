/*=========================================================================*\
* openssl.c
* lua-openssl binding
*
* This product includes PHP software, freely available from <http://www.php.net/software/>
* Author:  george zhao <zhaozg(at)gmail.com>
\*=========================================================================*/
#include "openssl.h"
#include <openssl/ssl.h>
#include <openssl/asn1.h>
#include <openssl/engine.h>
#include <openssl/opensslconf.h>
#include "private.h"

static int openssl_version(lua_State*L)
{
  lua_pushstring(L, LOPENSSL_VERSION_STR);
  lua_pushstring(L, LUA_VERSION);
  lua_pushstring(L, OPENSSL_VERSION_TEXT);
  return 3;
}

static LUA_FUNCTION(openssl_hex)
{
  size_t l = 0;
  const char* s = luaL_checklstring(L, 1, &l);
  int hl = 0;
  int encode = lua_isnoneornil(L, 2) ? 1 : lua_toboolean(L, 2);
  char* h = NULL;
  BIGNUM *B = NULL;

  if (encode)
  {
    B = BN_bin2bn((const unsigned char*)s, (int)l, NULL);
    h = BN_bn2hex(B);
    strlwr(h);
    hl = strlen(h);
  }
  else
  {
    BN_hex2bn(&B, s);
    hl = l / 2;
    h = OPENSSL_malloc( hl + 1 );
    BN_bn2bin(B, (unsigned char*)h);
    h[hl] = '\0';
  };
  lua_pushlstring(L, (const char*)h, hl);
  OPENSSL_free(h);
  BN_free(B);

  return 1;
}

static void list_callback(const OBJ_NAME *obj, void *arg)
{
  lua_State *L = (lua_State *)arg;
  int idx = (int)lua_objlen(L, -1);
  lua_pushstring(L, obj->name);
  lua_rawseti(L, -2, idx + 1);
}

static LUA_FUNCTION(openssl_list)
{
  static int options[] =
  {
    OBJ_NAME_TYPE_MD_METH,
    OBJ_NAME_TYPE_CIPHER_METH,
    OBJ_NAME_TYPE_PKEY_METH,
    OBJ_NAME_TYPE_COMP_METH
  };
  static const char *names[] = {"digests", "ciphers", "pkeys", "comps", NULL};
  int type = auxiliar_checkoption (L, 1, NULL, names, options);
  lua_createtable(L, 0, 0);
  OBJ_NAME_do_all_sorted(type, list_callback, L);
  return 1;
}

static LUA_FUNCTION(openssl_error_string)
{
  char buf[1024];
  unsigned long val;
  int verbose = lua_toboolean(L, 1);
  int ret = 0;
  val = ERR_get_error();
  if (val)
  {
    lua_pushinteger(L, val);
    ERR_error_string_n(val, buf, sizeof(buf));
    lua_pushstring(L, buf);
    ret = 2;
  }
  if (verbose)
  {
    ERR_print_errors_fp(stderr);
  }
  ERR_clear_error();

  return ret;
}

static int openssl_random_load(lua_State*L)
{
  const char *file = luaL_optstring(L, 1, NULL);
  char buffer[MAX_PATH];

  if (file == NULL)
    file = RAND_file_name(buffer, sizeof buffer);
  else if (RAND_egd(file) > 0)
  {
    /* we try if the given filename is an EGD socket.
       if it is, we don't write anything back to the file. */;
    lua_pushboolean(L, 1);
    return 1;
  }

  if (file == NULL || !RAND_load_file(file, 2048))
  {
    return openssl_pushresult(L, 0);
  }

  lua_pushboolean(L, RAND_status());
  return 1;
}

static int openssl_random_write(lua_State *L)
{
  const char *file = luaL_optstring(L, 1, NULL);
  char buffer[MAX_PATH];
  int n;

  if (!file && !(file = RAND_file_name(buffer, sizeof buffer)))
    return openssl_pushresult(L, 0);

  n = RAND_write_file(file);
  return openssl_pushresult(L, 1);
}

static int openssl_random_status(lua_State *L)
{
  lua_pushboolean(L, RAND_status());
  return 1;
}

static int openssl_random_cleanup(lua_State *L)
{
  RAND_cleanup();
  return 0;
}

static LUA_FUNCTION(openssl_random_bytes)
{
  long length = luaL_checkint(L, 1);
  int strong = lua_isnil(L, 2) ? 0 : lua_toboolean(L, 2);

  char *buffer = NULL;
  int ret = 0;

  if (length <= 0)
  {
    luaL_argerror(L, 1, "must greater than 0");
  }

  buffer = malloc(length + 1);
  if (strong)
  {
    ret = RAND_bytes((byte*)buffer, length);
    if (ret == 1)
    {
      lua_pushlstring(L, buffer, length);
    }
    else
    {
      lua_pushboolean(L, 0);
    }
  }
  else
  {
    ret = RAND_pseudo_bytes((byte*)buffer, length);
    if (ret == 1)
    {
      lua_pushlstring(L, buffer, length);
    }
    else
    {
      lua_pushboolean(L, 0);
    }
  }
  free(buffer);
  return 1;
}

static int openssl_object(lua_State* L)
{
  if (lua_isnumber(L, 1))
  {
    int nid = luaL_checkint(L, 1);
    ASN1_OBJECT* obj = OBJ_nid2obj(nid);
    if (obj)
      PUSH_OBJECT(obj, "openssl.asn1_object");
    else
      lua_pushnil(L);
  }
  else
  {
    const char* oid  = luaL_checkstring(L, 1);
    if (lua_isnoneornil(L, 2))
    {
      const char* name = luaL_checkstring(L, 2);
      const char* alias = luaL_optstring(L, 3, name);
      if (OBJ_create(oid, name, alias) == NID_undef)
        lua_pushboolean(L, 0);
      else
        lua_pushboolean(L, 1);
    }
    else
    {
      int nid = OBJ_txt2nid(oid);
      if (nid != NID_undef)
      {
        ASN1_OBJECT* obj = OBJ_nid2obj(nid);
        if (obj)
          PUSH_OBJECT(obj, "openssl.asn1_object");
        else
          lua_pushnil(L);
      }
      else
        lua_pushnil(L);
    }
  }
  return 1;
}

static int openssl_mem_leaks(lua_State*L)
{
  BIO *bio = BIO_new(BIO_s_mem());
  BUF_MEM* mem;

  CRYPTO_mem_leaks(bio);
  BIO_get_mem_ptr(bio, &mem);
  lua_pushlstring(L, mem->data, mem->length);
  BIO_free(bio);
  return 1;
}

static const luaL_Reg eay_functions[] =
{
  {"version",     openssl_version},
  {"list",        openssl_list},
  {"hex",         openssl_hex},
  {"mem_leaks",   openssl_mem_leaks},

  {"rand_status", openssl_random_status},
  {"rand_load",   openssl_random_load},
  {"rand_write",  openssl_random_write},
  {"rand_cleanup",openssl_random_cleanup},
  {"random",      openssl_random_bytes},

  {"error",       openssl_error_string},
  {"object",      openssl_object},

  {"engine",      openssl_engine},

  {NULL, NULL}
};

void CRYPTO_thread_setup(void);
void CRYPTO_thread_cleanup(void);

LUA_API int luaopen_openssl(lua_State*L)
{
  CRYPTO_thread_setup();

  OpenSSL_add_all_ciphers();
  OpenSSL_add_all_digests();
  SSL_library_init();

  ERR_load_ERR_strings();
  ERR_load_EVP_strings();


  ENGINE_load_dynamic();
  ENGINE_load_openssl();
#ifdef LOAD_ENGINE_CUSTOM
  LOAD_ENGINE_CUSTOM();
#endif
#ifdef OPENSSL_SYS_WINDOWS
  RAND_screen();
#endif

#if LUA_VERSION_NUM==501
  luaL_register(L, "openssl", eay_functions);
#elif LUA_VERSION_NUM==502
  lua_newtable(L);
  luaL_setfuncs(L, eay_functions, 0);
#endif
  openssl_register_lhash(L);
  openssl_register_engine(L);

  luaopen_bio(L);
  lua_setfield(L, -2, "bio");

  luaopen_asn1(L);
  lua_setfield(L, -2, "asn1");


  luaopen_digest(L);
  lua_setfield(L, -2, "digest");

  luaopen_cipher(L);
  lua_setfield(L, -2, "cipher");

  luaopen_hmac(L);
  lua_setfield(L, -2, "hmac");

  luaopen_pkey(L);
  lua_setfield(L, -2, "pkey");

#ifdef EVP_PKEY_EC
  luaopen_ec(L);
  lua_setfield(L, -2, "ec");
#endif

  luaopen_x509(L);
  lua_setfield(L, -2, "x509");

  luaopen_pkcs7(L);
  lua_setfield(L, -2, "pkcs7");

  luaopen_pkcs12(L);
  lua_setfield(L, -2, "pkcs12");

  luaopen_csr(L);
  lua_setfield(L, -2, "csr");

  luaopen_crl(L);
  lua_setfield(L, -2, "crl");

  luaopen_ocsp(L);
  lua_setfield(L, -2, "ocsp");

#ifdef OPENSSL_HAVE_TS
  /* timestamp handling */
  luaopen_ts(L);
  lua_setfield(L, -2, "ts");
#endif

  luaopen_cms(L);
  lua_setfield(L, -2, "cms");

  luaopen_ssl(L);
  lua_setfield(L, -2, "ssl");

  /* third part */
  luaopen_bn(L);
  lua_setfield(L, -2, "bn");

  luaopen_rsa(L);
  luaopen_dsa(L);
  luaopen_dh(L);

  return 1;
}

