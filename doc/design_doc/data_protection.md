# Data Encryption and Password Management

This document describes how Mozc obfuscates user data and how it works, in each platform.

## Background

Input methods stores some internal data in the local storage.  Most are stored in plain text, but Mozc applies AES256 CBC to obfuscate some data that are considered to be important for user privacy.

## Overview

As is described above, most of data are stored in plain text.  Only the following data are obfuscated with AES256 CBC:

  * User history: stores some of users input text, which is used to suggest text from the users history.
  * client ID: the ID to be marked for usage stats.  Not used if the user does not opt-in the "sending usage stats" configuration.

## Detailed Design

We have three policies for obfuscation:

  * We do not use a product-wide "master key".  Instead, each key of data encryption is different on each machine.  We want to prevent "casual" leaks such like uploading/publishing on the web, so "no product-wide common keys".
  * We trusts processes which runs with the user ID.  That is, if some malicious cracker succeeds to inject a virus on the machine and it knows Mozc quite well, the data encryption framework can't help.
  * Again, we want to prevent only "casual" leaks.  If you really want to protect your data, you should enable file-system encryption at OS-level.

Then, we take the following process:

  * Mozc firstly created a "password file", which stores the key and salt of AES256 CBC.
  * Then apply AES256 CBC for the data to be obfuscated.

Then, how to protect the "password file"?  It's up to the platform.

  * Windows: Uses [CryptProtectData](http://msdn.microsoft.com/en-us/library/windows/desktop/aa380261.aspx) and [CryptUnprotectData](http://msdn.microsoft.com/en-us/library/windows/desktop/aa380882.aspx) APIs, which guarantee the data is encrypted with keys of user credential and the machine id.  It means, the password file has to be differently encrypted if the user id is different and/or the machine is different.
  * Mac: The password file itself is also obfuscated with AES256 CBC, where the key and salt is generated from the machine's serial number and the user's ID in a predictable way, which means that anyone who knows the machine's serial number and the user's ID can read the password file.
  * Linux: no protection.  The key and salt are stored as plain data. Anyone who has the OS-level permission to reed the password file can read it.
  * ChromeOS: same as Linux.  But no protection does not cause a serious issue because the OS encrypts the local disk and no normal processes run in ChromeOS.
  * Android: Same as Linux.  But no protection does not cause a serious issue because other applications do not have the OS-level permission to read the Mozc's file.

## Code Location

  * [src/base/encryptor.cc](../../src/base/encryptor.cc): consists of two code logic of this obfuscation mechanism. One is the platform-neutral AES256 CBC logic and the other is platform-dependent protection logic for the password file.
  * [src/base/password_manager.cc](../../src/base/password_manager.cc): the password management (dealing with the password file). The actual data protection logic for the password file is delegated to encruptor.cc.
  * [src/base/unverified_aes256.cc](../../src/base/unverified_aes256.cc): the AES256 CBC implementation. Our implementation hasn't been verified with any authority such as FIPS.  Please do not use this implementation for any security/privacy critical purpose.
  * [src/base/unverified_sha1.cc](../../src/base/unverified_sha1.cc): the SHA1 implementation. Our implementation hasn't been verified with any authority such as FIPS.  Please do not use this implementation for any security/privacy critical purpose.

## Guide to port Mozc to a new platform

Before starting your work, you have to understand the privacy concern described in this document.  You also need to find out the best way to store password, for your platform.  For example, gnome-keyrings might be a choice for Linux implementation.
