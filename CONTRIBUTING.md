# Contributing to HydroCoupleOgc

Thank you for your interest in contributing. HydroCoupleOgc is a small,
Qt-only library that speaks the OGC web service protocols — WMS, WMTS, WFS and
WCS — on behalf of two larger applications. Its smallness is a feature, and
most of what follows exists to keep it that way.

This guide follows the governance model of
[openswmm.engine](https://github.com/HydroCouple/openswmm.engine), so a
contributor to either project should find the other familiar. Where it differs,
it differs because this project is genuinely different, and the difference is
stated rather than left implicit.

## Table of Contents

1. [Community Governance](#1-community-governance)
2. [Repository & Technical Management](#2-repository--technical-management)
3. [Succession & Delegation](#3-succession--delegation)
4. [Licensing, Intellectual Property & CLA](#4-licensing-intellectual-property--cla)
5. [Author Acknowledgment](#5-author-acknowledgment)
6. [What Belongs Here](#6-what-belongs-here)
7. [Versioning & API Stability](#7-versioning--api-stability)
8. [Branching Model](#8-branching-model)
9. [Bug Fixes & Small Improvements](#9-bug-fixes--small-improvements)
10. [Pull Request Process](#10-pull-request-process)
11. [Continuous Integration](#11-continuous-integration)
12. [How This Project Tests](#12-how-this-project-tests)
13. [Test Fixtures](#13-test-fixtures)
14. [Issue Reporting](#14-issue-reporting)
15. [Review Timeline Expectations](#15-review-timeline-expectations)
16. [Documentation Standards](#16-documentation-standards)
17. [Dependency Evaluation Policy](#17-dependency-evaluation-policy)
18. [Security Vulnerability Reporting](#18-security-vulnerability-reporting)
19. [Code of Conduct](#19-code-of-conduct)

---

## 1. Community Governance

HydroCoupleOgc is a community-driven open source project. All contributors are
welcome regardless of affiliation, background, or experience level. The
community operates on a model of open discussion, merit-based evaluation, and
transparent decision-making. Key decisions affecting the project direction are
made openly on GitHub, and all community members are encouraged to participate.

---

## 2. Repository & Technical Management

**Repository and technical management is the responsibility of the Technical
Manager (currently [@cbuahin](https://github.com/cbuahin)).**

The Technical Manager is responsible for:

- Maintaining the integrity and health of the `main` and `develop` branches.
- Setting and enforcing coding standards, testing requirements, and
  documentation guidelines.
- Triaging issues and pull requests in a timely manner.
- Making final decisions on merges, releases, and branch management.
- Keeping the library's API in step with its consumers, since a change here
  reaches both of them.

Community members who wish to take on elevated responsibilities may express
interest by opening a discussion on GitHub.

---

## 3. Succession & Delegation

The Technical Manager role is a single point of authority, which must be
protected against absence or unavailability.

- If the Technical Manager is unavailable for an extended period, they may
  designate a **Temporary Delegate** with equivalent merge and release
  authority. The delegation will be announced publicly in GitHub Discussions.
- If the Technical Manager steps down permanently, the outgoing manager will
  nominate a successor from the active contributor community. The nomination is
  subject to a community feedback period of at least two weeks before taking
  effect.
- In the absence of a named delegate and in urgent situations (for example a
  critical security fix), any two senior community reviewers may jointly
  approve and merge a patch to `main`, with a written rationale posted to
  GitHub Discussions immediately afterward.

---

## 4. Licensing, Intellectual Property & CLA

HydroCoupleOgc is released under the **GNU Lesser General Public License,
version 3 or later**. Attribution requirements are recorded in
[NOTICE](./NOTICE).

The licence is weak copyleft by design. This library is linked by
HydroCoupleComposer under the LGPL and by openswmm.gui under the GPL, and the
LGPL is what lets one shared library serve both. **A change of outbound licence
here would break one of those two consumers**, so it is not a routine decision.

All contributors must sign the project **Contributor License Agreement (CLA)**
before their pull request can be merged. The CLA is detailed in
[CLA.md](./CLA.md). Key points:

- **You retain your copyright.** The CLA grants a licence; it does not transfer
  ownership.
- **Broad licence grant.** You grant the Technical Manager a perpetual,
  irrevocable licence to use, distribute, and **relicense** your contribution.
- **Patent grant.** You grant a royalty-free patent licence covering patents
  necessarily infringed by your contribution.
- **Representation of authority.** You confirm you have the right to submit the
  contribution and that it contains no unlicensed third-party material.
- **Corporate contributors** must additionally submit a Corporate CLA — see
  [CLA.md §6](./CLA.md#6-corporate-contributors).

### Signing the CLA

The project uses [CLA Assistant](https://cla-assistant.io). When you open your
first pull request, a bot will post a comment with a signing link. You may also
sign manually by posting the following comment on your PR:

> I have read the CLA Document and I hereby sign the CLA.

Once signed, the CLA covers all future contributions.

If your contribution includes third-party code or data, you are responsible for
ensuring its licence is compatible with the LGPL-3.0-or-later and that proper
attribution is added to [NOTICE](./NOTICE).

---

## 5. Author Acknowledgment

All contributors whose work is incorporated into HydroCoupleOgc will be
recognized in [AUTHORS.md](./AUTHORS.md).

- **Name, affiliation (if any), and scope of contribution** will be documented
  for each contributor.
- The Technical Manager maintains `AUTHORS.md` and will update it at each
  release.
- If you believe your contribution has been omitted or mis-described, please
  open an issue or say so on the pull request.

Contributors are encouraged to add themselves to `AUTHORS.md` as part of their
pull request.

---

## 6. What Belongs Here

This library is deliberately narrow, and the boundary is worth stating because
it is easy to erode one useful addition at a time.

**In scope.** Protocol work that is the same for every application: reading
capabilities documents, building request URLs, resolving CRS identifiers and
axis order, negotiating versions, and fetching bytes over HTTP with
deduplication, cancellation and credentials.

**Out of scope.** Anything that renders, decodes, projects or presents. Those
belong to the consuming application, whose layer contracts differ from each
other — HydroCoupleComposer's `MapLayer` and openswmm.gui's `OpenSWMMVisLayer`
are unrelated types, and the attempt to unify them is what this split exists to
avoid.

**The dividing test.** If it needs GDAL, Qt Widgets, or a canvas, it belongs in
the consumer. If it takes bytes and returns data, it belongs here.

**GDAL is deliberately absent**, and not by oversight. In both consuming
projects GDAL is built without curl, so its WMS, WCS and WFS drivers are not
registered and `CPLHTTPFetch` is a stub. GDAL can decode bytes it is handed —
that is how a fetched GeoTIFF or GeoJSON becomes a layer — but it cannot fetch
them. That is the whole reason this library exists. Do not add GDAL here.

---

## 7. Versioning & API Stability

HydroCoupleOgc follows **Semantic Versioning** as defined at
[semver.org](https://semver.org).

| Component | When to increment |
|-----------|-------------------|
| `MAJOR`   | Incompatible API or ABI changes |
| `MINOR`   | New capability added in a backward-compatible manner |
| `PATCH`   | Backward-compatible bug fixes, documentation, or performance work |

API stability matters more here than the size of the library suggests. Both
consumers currently take this project through CMake `FetchContent` pinned to
the `main` branch rather than to a tag, so **a breaking change on `main`
reaches them at their next configure**, not at a release they chose. Until the
first tagged release:

- Treat every change to a public header as though a consumer will pick it up
  tomorrow, because one will.
- A change that breaks a consumer should land together with the corresponding
  change in that consumer, not before it.
- Prefer adding to a struct over changing the meaning of an existing field.

---

## 8. Branching Model

| Branch                   | Purpose |
|--------------------------|---------|
| `main`                   | Stable. What both consumers fetch. |
| `develop`                | Integration branch for ongoing development. |
| `feature/<name>`         | Short-lived branches for feature work, forked from `develop`. |
| `bugfix/<issue-id>-desc` | Short-lived branches addressing a specific report. |
| `release/<version>`      | Release preparation, cut from `develop`. |

---

## 9. Bug Fixes & Small Improvements

1. **Open or reference an issue.** Verify the bug is reproducible and link the
   issue number in subsequent commits and pull requests.
2. **Fork `develop`.** Name your branch `bugfix/<issue-id>-short-description`.
3. **Write a failing test first.** It must demonstrate the defect before the
   fix is applied. If the defect is one a real server exposes, save that
   server's response as a fixture — see [§13](#13-test-fixtures).
4. **Implement the minimal fix.** Touch only what the issue requires. Do not
   refactor adjacent code.
5. **Run the whole suite.** See [§12](#12-how-this-project-tests).
6. **Submit a pull request** against `develop`.

---

## 10. Pull Request Process

### Required Approvals

| Reviewer | Role |
|----------|------|
| **Technical Manager** | [@cbuahin](https://github.com/cbuahin) — final authority on code quality, architecture, and correctness. Required. |
| **Community Reviewer** | At least one contributor other than the author. Required once the project has two or more active contributors; until then, encouraged and requested where a reviewer is available. |

Self-approvals are not permitted. This differs from openswmm.engine, which
requires a community reviewer on every pull request: that rule is right for a
project with a contributor base and unenforceable for one without, and a rule
that cannot be followed is worse than one honestly scoped.

### PR Checklist

- [ ] Targets `develop`
- [ ] CLA signed
- [ ] Describes what changed and why
- [ ] References related issues (e.g. `Closes #42`)
- [ ] Includes tests covering the change, named for the behaviour they hold
- [ ] Includes a falsification pass for new behaviour — see [§12](#12-how-this-project-tests)
- [ ] Full suite passes locally
- [ ] CI passes
- [ ] Public headers documented (see [§16](#16-documentation-standards))
- [ ] `AUTHORS.md` updated if this is your first contribution
- [ ] If a public header changed, says which consumer is affected and how

### Merge Policy

- Merges into `main` are performed by the Technical Manager.
- Squash merging is preferred for `bugfix` and `feature` branches.

---

## 11. Continuous Integration

Three workflows run in GitHub Actions. A failing pipeline blocks merge
regardless of approvals.

| Workflow | What it does |
|----------|--------------|
| `unit_testing.yml` | Builds and runs the full suite on Linux, macOS and Windows. Qt comes from a Qt installation; vcpkg supplies Google Test alone. |
| `documentation.yml` | Builds the Doxygen site and, on `main` and `develop`, deploys it to GitHub Pages. Pull requests build the docs but do not deploy them. |
| `deployment.yml` | Packages the install tree per platform, and creates a GitHub Release on a `v*.*.*` tag. |

Run the suite locally before opening a pull request. Every test here is
offline, so there is no reason not to.

---

## 12. How This Project Tests

Two things are asked of a test in this repository, and the second is the one
people are not expecting.

**Name the behaviour, not the function.** A test is called
`aProjectedCoverageNamesItsAxesXAndY`, not `testParseDescribe`. The suite
should read as a description of what servers do and what this library does
about it.

**Show that the test can fail.** New behaviour arrives with a falsification
pass: a script under `verification/` that breaks the implementation in specific
ways and records, for each mutation, whether the suite caught it. A gate that
survives every plausible break is not protecting anything.

A surviving mutation is a finding, and usually the code is what is wrong. The
existing scripts are worth reading before writing one; each mutation is a
sentence describing a mistake a reasonable person would make.

Where a mutation genuinely cannot be caught, say so in the script rather than
deleting it or weakening the claim. There are such cases here, and they are
recorded as such.

**No test touches the network.** The suites run against saved responses, and
the one HTTP test starts a server inside the test process on loopback. A
contribution that reaches a real service from a test will be asked to change,
because a test that depends on someone else's uptime gets switched off within a
week of its first false failure.

`verification/` is untracked working material, not part of the suite. Include
the script's output in the pull request description.

---

## 13. Test Fixtures

The fixtures under `tests/fixtures/` are the evidence base of this library, and
their integrity is a licensing matter as well as a technical one — see
[CLA.md §5.4](./CLA.md#5-representations-and-warranties).

A fixture must be:

- **Verbatim.** The server's own bytes, unedited. A fixture trimmed to make a
  test pass no longer records what a server does, which is the only reason to
  have it.
- **Openly published.** From a service that publishes the document without
  authentication.
- **Clean.** No credentials, API keys, tokens, or personal data.
- **Attributed.** Add the service to the fixture list in [NOTICE](./NOTICE).

Name a fixture for the protocol, version and source:
`wcs-2.0.1-pdok-ahn-describe.xml`. Prefer a real oddity over a tidy example —
the fixtures that have earned their place here are the ones that surprised
somebody.

If a document is too large to commit whole, trim it by removing entire
elements, never by altering values, and say in the pull request what was
removed.

---

## 14. Issue Reporting

### Bug Reports

Include:

- What you asked the service for, and which service. A URL is ideal; the
  capabilities document is better.
- What came back, and what you expected.
- The library version or commit, the Qt version, and the platform.
- Whether it reproduces against a second server. Many OGC defects are one
  implementation's interpretation rather than a protocol matter, and knowing
  which changes the fix.

### Feature Requests

Say which protocol and version, and if possible name a public server that
supports it. A capability nobody deploys is hard to justify testing.

---

## 15. Review Timeline Expectations

Targets, not guarantees:

| Action | Target |
|--------|--------|
| Initial issue triage | Within 2 weeks |
| First review response on a pull request | Within 3 weeks |
| Follow-up review after requested changes | Within 2 weeks |

If you have not heard back within the stated window, a polite follow-up comment
is welcome. Pull requests with no activity for 60 days may be closed with a
note that they can be reopened.

---

## 16. Documentation Standards

The project uses **Doxygen**. All public code elements must carry documentation
comments, and the documentation build must be free of warnings about them.

Per element:

- `\brief` — one line, saying what it is for rather than restating its name.
- `\param` — each parameter, including units and the spelling expected where
  that matters (CRS identifiers, axis labels, format strings).
- `\returns` — including what an empty or failed result means.
- `\note` / `\warning` — behavioural caveats.

Beyond the tags, this codebase documents **why**, and particularly why
something is not the obvious thing. Where the code accommodates a real
server's behaviour, say which server and what it does. Those comments are the
reason the next person does not undo the fix.

---

## 17. Dependency Evaluation Policy

A dependency added here is added to both consumers, neither of which agreed to
it. The bar is correspondingly high.

| Criterion | Requirement |
|-----------|-------------|
| **Necessity** | The need cannot be met by Qt Core or Qt Network, and cannot be met in the consumer instead. |
| **Licence compatibility** | Compatible with LGPL-3.0-or-later, and compatible with distribution inside both an LGPL and a GPL application. |
| **Consumer impact** | Both consumers can obtain it. Something available only through one project's package manager is not a candidate. |
| **Maintenance status** | Actively maintained, with a responsive upstream. |
| **Stability** | A stable, versioned API. Unpinned dependencies are not acceptable. |
| **Platform support** | Linux, macOS and Windows. |

Raise a proposal in a GitHub Discussion or issue before implementing against a
new dependency. Test-only dependencies, declared behind the `tests` feature in
`vcpkg.json`, face lower scrutiny because they reach no consumer — but they
still have to be licence-compatible.

The current answer is Qt and nothing else, plus Google Test for the suites.
That is not an accident and it should take a real argument to change.

---

## 18. Security Vulnerability Reporting

**Do not report security vulnerabilities through public GitHub issues.**

Report privately through the
[Security Advisories](https://github.com/HydroCouple/HydroCoupleOgc/security/advisories/new)
tab, tagging [@cbuahin](https://github.com/cbuahin). Include:

- A description of the vulnerability and its potential impact.
- Steps to reproduce, including any relevant documents or code.
- The version or commit affected.
- Any known mitigations.

This library makes network requests on a user's behalf and carries their
credentials while doing it, so the following are in scope and worth reporting
even when they seem minor: credentials leaking into logs, error messages, or
URLs recorded anywhere; a redirect or a service-supplied URL causing a request
to go somewhere the user did not intend; and anything that causes a credential
to be attached to a host other than the one it was entered for.

The Technical Manager will acknowledge receipt within **five business days**
and work with you on a coordinated disclosure timeline. Credit will be given in
the release notes and advisory unless you prefer anonymity.

---

## 19. Code of Conduct

All participants are expected to follow the project
[Code of Conduct](./CODE_OF_CONDUCT.md), which sets out both the behaviour
expected in project spaces and how to report a violation.

---

*This document is maintained by the Technical Manager of HydroCoupleOgc
(currently [@cbuahin](https://github.com/cbuahin)). Last updated: August 2026.*
