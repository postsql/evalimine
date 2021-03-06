/*
 * Copyright: Eesti Vabariigi Valimiskomisjon
 * (Estonian National Electoral Committee), www.vvk.ee
 * Derived work from libdicidocpp library
 * https://svn.eesti.ee/projektid/idkaart_public/trunk/libdigidocpp/
 * Written in 2011-2013 by Cybernetica AS, www.cyber.ee
 *
 * This work is licensed under the Creative Commons
 * Attribution-NonCommercial-NoDerivs 3.0 Unported License.
 * To view a copy of this license, visit
 * http://creativecommons.org/licenses/by-nc-nd/3.0/.
 * */

#include "PyBDoc.h"
#include "BDoc.h"
#include "Signature.h"
#include "StackException.h"
#include "ChallengeVerifierImpl.h"
#include <xsec/utils/XSECPlatformUtils.hpp>
#include "crypto/OpenSSLHelpers.h"
#include "crypto/X509Cert.h"
#include <openssl/err.h>

void __composeNewSignature(BDocVerifierResult *res, const std::string& sig)
{
	res->signature = sig;
}

void __composeResultInfo(BDocVerifierResult *res, bdoc::Signature *sig,
			const char* xml, size_t xml_len)
{
	res->subject = sig->getSubject();
	res->ocsp_time = sig->getProducedAt();
	res->signature = std::string(xml, xml_len);
}

void __composeResultErrorInfo(BDocVerifierResult *res, const char *exc)
{
	res->result = false;
	res->error = exc;
}

void __composeResultErrorInfo(
	BDocVerifierResult *res, bdoc::StackExceptionBase& exc)
{
	res->result = false;
	res->error = exc.what();
}

void __composeResultErrorInfo(BDocVerifierResult *res, std::exception& exc)
{
	res->result = false;
	res->error = exc.what();
}

void __composeResultErrorInfo(BDocVerifierResult *res)
{
	res->result = false;
	res->error = "Unknown (...) error";
}

//

void initialize() {
	SSL_load_error_strings();
	SSL_library_init();
	OPENSSL_config(NULL);
	xercesc::XMLPlatformUtils::Initialize();
	XSECPlatformUtils::Initialise();
}

void terminate() {
	XSECPlatformUtils::Terminate();
	xercesc::XMLPlatformUtils::Terminate();
}

std::list<std::string> list_policies(const unsigned char *buf, size_t len)
{
	bdoc::X509Cert cert(buf, len);
	return cert.policies();
}

//
//
//

CertificateData::CertificateData() :
	subject_name(), issuer_name(), serial_number(), modulus(), exponent()
{
}

CertificateData::CertificateData(const CertificateData& cd) :
	subject_name(cd.subject_name), issuer_name(cd.issuer_name),
	serial_number(cd.serial_number), modulus(cd.modulus), exponent(cd.exponent)
{
}

CertificateData& CertificateData::operator=(const CertificateData& cd)
{
	if (this != &cd) {
		subject_name = cd.subject_name;
		issuer_name = cd.issuer_name;
		serial_number = cd.serial_number;
		modulus = cd.modulus;
		exponent = cd.exponent;
	}
	return *this;
}

CertificateData::~CertificateData()
{
}

void CertificateData::decode(const unsigned char *buf, size_t len)
{
	bdoc::X509Cert cert(buf, len);
	subject_name = cert.getSubject();
	issuer_name = cert.getIssuerName();
	serial_number = cert.getSerial();
	modulus = cert.getRsaModulus();
	exponent = cert.getRsaExponent();
}

//
//
//

BDocVerifierResult::BDocVerifierResult() :
	result(true),
	subject(),
	ocsp_time(),
	error(),
	signature(),
	cert_is_valid(true),
	ocsp_is_good(true)
{
}

BDocVerifierResult::BDocVerifierResult(const BDocVerifierResult& bvr) :
	result(bvr.result),
	subject(bvr.subject),
	ocsp_time(bvr.ocsp_time),
	error(bvr.error),
	signature(bvr.signature),
	cert_is_valid(bvr.cert_is_valid),
	ocsp_is_good(bvr.ocsp_is_good)
{
}

BDocVerifierResult&
	BDocVerifierResult::operator=(const BDocVerifierResult& bvr)
{
	if (this != &bvr) {
		result = bvr.result;
		subject = bvr.subject;
		ocsp_time = bvr.ocsp_time;
		error = bvr.error;
		signature = bvr.signature;
		cert_is_valid = bvr.cert_is_valid;
		ocsp_is_good = bvr.ocsp_is_good;
	}
	return *this;
}

BDocVerifierResult::~BDocVerifierResult()
{
}

//
//
//

BDocVerifier::BDocVerifier() :
	bdoc(NULL),
	conf(NULL)
{
	bdoc = new bdoc::ContainerInfo();
	conf = new bdoc::Configuration();
}

BDocVerifier::~BDocVerifier()
{
	delete bdoc;
	delete conf;
}

void BDocVerifier::setSchemaDir(const char *path)
{
	conf->setSchemaDir(path);
}

void BDocVerifier::addCertToStore(const char *path)
{
	conf->addCertToStore(path);
}

void BDocVerifier::setDocument(
	const unsigned char *buf, size_t len, const char* uri)
{
	bdoc->setDocument(uri, buf, len);
}

void BDocVerifier::addOCSPConf(
	const char *issuer, const char *url,
	const char *cert, long skew, long maxAge)
{
	conf->addOCSPConf(issuer, url, cert, skew, maxAge);
}

void BDocVerifier::setDigestURI(const char *uri)
{
	conf->setDigestURI(uri);
}

const BDocVerifierResult BDocVerifier::verifyBESOffline(
					const char* xml, size_t xml_len)
{
	BDocVerifierResult res;
	try {
		std::auto_ptr<bdoc::Signature> sig(bdoc::Signature::parse(conf->getSchemaDir(), xml, xml_len, bdoc));
		__composeResultInfo(&res, sig.get(), xml, xml_len);
		sig->validateOffline(conf->getCertStore());

		bdoc::X509Cert x509 = sig->getSigningCertificate();
		if (!x509.isValid()) {
			__composeResultErrorInfo(&res, "Certificate is expired");
			res.cert_is_valid = false;
		}
	}
	catch (bdoc::StackExceptionBase& exc) {
		__composeResultErrorInfo(&res, exc);
	}
	catch (std::exception& exc) {
		__composeResultErrorInfo(&res, exc);
	}
	catch (...) {
		__composeResultErrorInfo(&res);
	}
	return res;
}

const BDocVerifierResult BDocVerifier::verifyBESOnline(
					const char* xml, size_t xml_len)
{
	BDocVerifierResult res;
	try {
		std::auto_ptr<bdoc::Signature> sig(bdoc::Signature::parse(conf->getSchemaDir(), xml, xml_len, bdoc));
		__composeResultInfo(&res, sig.get(), xml, xml_len);
		sig->validateOffline(conf->getCertStore());

		bdoc::SignatureValidator sv(sig.get(), conf);
		bdoc::OCSP::CertStatus status = sv.validateBESOnline();

		res.ocsp_is_good = false;
		switch (status) {

			case bdoc::OCSP::GOOD:
				__composeNewSignature(&res,
							sv.getTMSignature());

				res.ocsp_time = sv.getProducedAt();
				res.ocsp_is_good = true;
				break;

			case bdoc::OCSP::REVOKED:
				__composeResultErrorInfo(&res,
						"Certificate status revoked");
				break;

			case bdoc::OCSP::UNKNOWN:
				__composeResultErrorInfo(&res,
						"Certificate status unknown");
				break;

			default:
				__composeResultErrorInfo(&res,
						"Certificate status invalid");
				break;
		}
	}
	catch (bdoc::StackExceptionBase& exc) {
		__composeResultErrorInfo(&res, exc);
	}
	catch (std::exception& exc) {
		__composeResultErrorInfo(&res, exc);
	}
	catch (...) {
		__composeResultErrorInfo(&res);
	}
	return res;
}

const BDocVerifierResult BDocVerifier::verifyTMOffline(
					const char* xml, size_t xml_len)
{
	BDocVerifierResult res;
	try {
		std::auto_ptr<bdoc::Signature> sig(bdoc::Signature::parse(conf->getSchemaDir(), xml, xml_len, bdoc));
		__composeResultInfo(&res, sig.get(), xml, xml_len);
		sig->validateOffline(conf->getCertStore());

		bdoc::SignatureValidator sv(sig.get(), conf);
		res.ocsp_is_good = false;
		sv.validateTMOffline();
		res.ocsp_is_good = true;
	}
	catch (bdoc::StackExceptionBase& exc) {
		__composeResultErrorInfo(&res, exc);
	}
	catch (std::exception& exc) {
		__composeResultErrorInfo(&res, exc);
	}
	catch (...) {
		__composeResultErrorInfo(&res);
	}
	return res;
}

/*
 * ChallengeVerifier
 * */


ChallengeVerifier::ChallengeVerifier() : error(), impl(NULL)
{
	impl = new ChallengeVerifierImpl();
}

ChallengeVerifier::~ChallengeVerifier()
{
	delete impl;
}

bool ChallengeVerifier::isChallengeOk()
{
	bool res = false;
	try {
		res = impl->isChallengeOk();
		if (!res) {
			error = impl->error;
		}
		return res;
	}
	catch (...) {
		error = "Unknown error";
		res = false;
	}
	return res;
}

void ChallengeVerifier::setCertificate(const unsigned char *buf, size_t len)
{
	impl->certificate.set(buf, len);
}

void ChallengeVerifier::setChallenge(const unsigned char *buf, size_t len)
{
	impl->challenge.set(buf, len);
}

void ChallengeVerifier::setSignature(const unsigned char *buf, size_t len)
{
	impl->signature.set(buf, len);
}

