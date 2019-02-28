# ![Euboea](logo.png)

Euboea is blazingly fast and small programming language **compiled** ahead-of-time **directly to machine code**.

## Builds

| Build             | Tests             | Misc |
|-------------------|-------------------|------|
| [![Build 1](https://travis-matrix-badges.herokuapp.com/repos/kspalaiologos/Euboea/branches/master/1)](https://travis-ci.org/kspalaiologos/Euboea) <br> [![wercker status](https://app.wercker.com/status/eead1e3f0f850024dd70ee1f6fc65b5f/m/master "wercker status")](https://app.wercker.com/project/byKey/eead1e3f0f850024dd70ee1f6fc65b5f) | Test build: [![Build 2](https://travis-matrix-badges.herokuapp.com/repos/kspalaiologos/Euboea/branches/master/2)](https://travis-ci.org/kspalaiologos/Euboea) <br> Cov build: [![Build 3](https://travis-matrix-badges.herokuapp.com/repos/kspalaiologos/Euboea/branches/master/3)](https://travis-ci.org/kspalaiologos/Euboea) <br> Coverage: [![Coverage status](https://codecov.io/gh/kspalaiologos/Euboea/branch/master/graph/badge.svg)](https://codecov.io/github/kspalaiologos/Euboea?branch=master) | [![CodeFactor](https://www.codefactor.io/repository/github/kamilaszewczyk/euboea/badge)](https://www.codefactor.io/repository/github/kamilaszewczyk/euboea) <br> [![Codacy Badge](https://api.codacy.com/project/badge/Grade/02ed01fb801d49a1b5e41bf244ad6971)](https://app.codacy.com/app/marekszuwarek1958/Euboea?utm_source=github.com&utm_medium=referral&utm_content=kspalaiologos/Euboea&utm_campaign=Badge_Grade_Dashboard) |



## Philosophy
Philosophy of Euboea includes following rules:

* **Fast** language compiled to machine code.
* Use **JIT** compilation.
* Include **minimal usable set of keywords and control structures**.
* Allow **simple cooperation with C**.
* **No access to files** on physical drive.
* Access only to **stream I/O**.
* Focus on targeting **UNIX-like** operating system.
* Intended to be used with **shell scripts**.
* Focus on **low level** programming.

## Speed

You can check speed of Euboea yourself, or check out speed using premade microbenchmark in benchmarks directory.
On my PC, Euboea can be up to **4 times faster than PUC-Rio Lua**

## Learning Euboea

You can check out the wiki! ![Link](https://github.com/kspalaiologos/Euboea/wiki)

## Contributing

Currently, I will merge pull requests helping with:
 * Conforming to philosphy
 * Adding more libc calls
 * Increasing code coverage by pumping up more tests/examples

[//]: # (Listening to https://www.youtube.com/watch?v=Dqzrofdwi-g once is one free hug to you)
