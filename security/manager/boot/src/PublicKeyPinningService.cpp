/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PublicKeyPinningService.h"
#include "StaticHPKPins.h" // autogenerated by genHPKPStaticpins.js
#include "ScopedNSSTypes.h"
#include "pkix/pkixtypes.h"

#include "nsString.h"
#include "cert.h"
#include "nssb64.h"
#include "prlog.h"
#include "seccomon.h"
#include "sechash.h"
#include "mozilla/Base64.h"

using namespace mozilla;
using namespace mozilla::psm;

#if defined(PR_LOGGING)
PRLogModuleInfo* gPublicKeyPinningLog =
  PR_NewLogModule("PublicKeyPinningService");
#endif

/**
 Computes in the location specified by base64Out the SHA256 digest
 of the DER Encoded subject Public Key Info for the given cert
*/
static SECStatus
GetBase64SHA256SPKI(const CERTCertificate* cert,
                    nsACString& aSha256SPKIDigest){
  aSha256SPKIDigest.Truncate();
  Digest digest;
  nsresult rv = digest.DigestBuf(SEC_OID_SHA256, cert->derPublicKey.data,
                                 cert->derPublicKey.len);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return SECFailure;
  }
  rv = Base64Encode(nsDependentCSubstring(
                      reinterpret_cast<const char*>(digest.get().data),
                      digest.get().len),
                      aSha256SPKIDigest);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return SECFailure;
  }
  return SECSuccess;
}

/**
 * Given a Pinset and certlist ensure that there is intersection between them
 */
static bool
EvalPinWithPinset(const CERTCertList* certList, const StaticPinset* pinSet) {
  SECStatus srv;
  CERTCertificate* currentCert;
  nsAutoCString base64Out;

  CERTCertListNode* node;
  for (node = CERT_LIST_HEAD(certList);
       !CERT_LIST_END(node, certList);
       node = CERT_LIST_NEXT(node)) {

    currentCert = node->cert;

    PR_LOG(gPublicKeyPinningLog, PR_LOG_DEBUG,
           ("pkpin: certArray subject:     '%s'\n",
            currentCert->subjectName));
    PR_LOG(gPublicKeyPinningLog, PR_LOG_DEBUG,
           ("pkpin: certArray common_name: '%s'\n",
            CERT_GetCommonName(&(currentCert->issuer))));

    // I only need sha256fp for builtins (so far)
    srv = GetBase64SHA256SPKI(currentCert, base64Out);
    if (srv != SECSuccess) {
      PR_LOG(gPublicKeyPinningLog, PR_LOG_DEBUG,
             ("pkpin: GetBase64SHA256SPKI failed!\n"));
      return false;
    }
    PR_LOG(gPublicKeyPinningLog, PR_LOG_DEBUG,
           ("pkpin: base_64(hash(key)='%s'\n", base64Out.get()));
    // Compare, linear search for now...
    for (size_t j = 0; j < pinSet->size; j++){
      if (base64Out.Equals(pinSet->data[j])) {
        PR_LOG(gPublicKeyPinningLog, PR_LOG_DEBUG,
               ("pkpin: found pin base_64(hash(key)='%s'\n", base64Out.get()));
        return true;
      }
    }
  }
  // We found no intersection, we must fail!
  PR_LOG(gPublicKeyPinningLog, PR_LOG_DEBUG, ("pkpin: end of evaluation!\n"));
  return false;
}

/**
  Comparator for the is public key pinned host.
*/
static int
TransportSecurityPreloadCompare(const void *key, const void *entry) {
  const char *keyStr = reinterpret_cast<const char *>(key);
  const TransportSecurityPreload *preloadEntry =
    reinterpret_cast<const TransportSecurityPreload *>(entry);

  return strcmp(keyStr, preloadEntry->mHost);
}

/**
 * Check PKPins on the given certlist against the specified hostname
 */
static bool
CheckPinsForHostname(const CERTCertList *certList, const char *hostname,
                     const PRTime time)
{
  if (!certList) {
    return false;
  }
  if (!hostname || hostname[0] == 0) {
    return false;
  }

  TransportSecurityPreload *foundEntry = nullptr;
  char *evalHost = const_cast<char*>(hostname);
  char *evalPart;
  // Notice how the (xx = strchr) prevents pins for unqualified domain names.
  while (!foundEntry && (evalPart = strchr(evalHost, '.'))) {
    PR_LOG(gPublicKeyPinningLog, PR_LOG_DEBUG,
           ("pkpin: Iteration Querying for pinning for host: '%s'\n",
                                         evalHost));
    foundEntry = (TransportSecurityPreload *)bsearch(evalHost,
                                      kPublicKeyPinningPreloadList,
                                      kPublicKeyPinningPreloadListLength,
                                      sizeof(TransportSecurityPreload),
                                      TransportSecurityPreloadCompare);
    if (foundEntry) {
      PR_LOG(gPublicKeyPinningLog, PR_LOG_DEBUG,
             ("pkpin: iteration found pinning for host: '%s'\n", evalHost));
      if (evalHost != hostname) {
        if (false == foundEntry->mIncludeSubdomains) {
          // Does not apply to this host, contninue iterating
          foundEntry = nullptr;
        }
      }
    }
    evalHost = evalPart;
    // Advance past the '.'
    evalHost++;
  } // end while

  if (foundEntry && foundEntry->pinset) {
    return EvalPinWithPinset(certList, foundEntry->pinset);
  }
  return true; // No pinning information for this hostname
}

/**
 * Extract all the DNS names for a host (including CN) and evaluate the
 * certifiate pins against all of them (Currently is an OR so we stop
 * evaluating at the first OK pin).
 */
static bool
CheckChainAgainstAllNames(const CERTCertList* certList, const PRTime time) {
  PR_LOG(gPublicKeyPinningLog, PR_LOG_DEBUG,
         ("pkpin: top of checkChainAgainstAllNames"));
  CERTCertListNode* node = CERT_LIST_HEAD(certList);
  if (!node) {
    return false;
  }
  CERTCertificate* cert = node->cert;
  if (!cert) {
    return false;
  }

  mozilla::pkix::ScopedPLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
  if (!arena) {
    return false;
  }

  bool hasValidPins = false;
  CERTGeneralName* nameList;
  CERTGeneralName* currentName;
  nameList = CERT_GetConstrainedCertificateNames(cert, arena.get(), PR_TRUE);
  if (!nameList) {
    return false;
  }

  currentName = nameList;
  do {
    if (currentName->type == certDNSName
        && currentName->name.other.data[0] != 0) {
      // no need to cleaup, as the arena cleanup will do
      char *hostName = (char *)PORT_ArenaAlloc(arena.get(),
                                               currentName->name.other.len + 1);
      if (!hostName) {
        break;
      }
      // We use a temporary buffer as the hostname as returned might not be
      // null terminated.
      hostName[currentName->name.other.len] = 0;
      memcpy(hostName, currentName->name.other.data,
             currentName->name.other.len);
      if (!hostName[0]) {
        // cannot call CheckPinsForHostname on empty or null hostname
        break;
      }
      if (CheckPinsForHostname(certList, hostName, time)) {
        hasValidPins = true;
        break;
      }
    }
    currentName = CERT_GetNextGeneralName(currentName);
  } while (currentName != nameList);

  return hasValidPins;
}

bool
PublicKeyPinningService::ChainHasValidPins(const CERTCertList* certList,
                                           const char* hostname,
                                           const PRTime time)
{
  if (!certList) {
    return false;
  }
  if (time > kPreloadPKPinsExpirationTime) {
    return true;
  }
  if (!hostname || hostname[0] == 0) {
    return CheckChainAgainstAllNames(certList, time);
  }
  return CheckPinsForHostname(certList, hostname, time);
}
