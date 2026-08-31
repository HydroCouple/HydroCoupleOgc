# Authors

HydroCoupleOgc is the OGC web service client library shared by
HydroCoupleComposer and openswmm.gui. The contributors below are recorded per
[CONTRIBUTING.md §5](./CONTRIBUTING.md#5-author-acknowledgment).

## Project Lead

- **Caleb Buahin** [@cbuahin](https://github.com/cbuahin) — Project lead,
  architect, and author of the library: the WMS, WMTS, WFS and WCS
  capabilities parsers and request builders, the shared HTTP tier
  (deduplication, cancellation, credentials), CRS URN resolution and axis
  order, service discovery, and the test suites and falsification scripts.

## Contributors

This is a new repository and the list is short. If your work is incorporated
and you are not named here, that is an oversight rather than a judgement —
open an issue or say so on the pull request and it will be corrected.

## AI-Assisted Development

- **Claude** (Anthropic) — Code generation, test and falsification-script
  authorship, and protocol research against live OGC services.

## Acknowledgements

The library is written against saved responses from public services rather
than against the specifications alone, because what the specifications permit
and what servers actually do are not the same thing. The suites here would be
considerably weaker without the organisations that publish open OGC endpoints;
they are credited individually in [NOTICE](./NOTICE).

Two of them shaped the design directly rather than merely supplying test data.
**rasdaman** refuses a subset whose axis label is not the coverage's own,
which is what established that WCS 2.0 axis names must be read from
DescribeCoverage rather than assumed. **PDOK** answers a request for one
protocol version with a document in another, and publishes 2.0 coverages
carrying an identifier and nothing else — both of which are now gated
behaviours.
