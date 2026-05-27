# Legal Notice

## Independent reverse-engineered implementation

This project is an **independent, clean-room reverse-engineered
implementation** of a serial communication protocol used by certain
industrial flowmeters from the mid-1990s.

This project:

- **Is not affiliated** with Endress+Hauser AG, Softing AG, or any
  other manufacturer of industrial process equipment
- **Is not endorsed by or sponsored by** any such manufacturer
- **Does not contain** any proprietary code, firmware, schematics, or
  confidential documentation from any manufacturer
- **Does not enable** copying, redistribution, or circumvention of any
  manufacturer's proprietary software (e.g., Commuwin II, FieldCare,
  DeviceCare)

## How this was developed

The protocol implementation in this repository was developed through
**black-box observation** of unencrypted traffic on a publicly
accessible serial bus, using equipment legally owned by the
developer. No instrument firmware was extracted, decompiled, or
analyzed.

This methodology is commonly known as "clean-room reverse engineering"
and is recognized as lawful in:

- **European Union:** Directive 2009/24/EC on the legal protection of
  computer programs, Article 6 (interoperability exception)
- **United States:** 17 U.S.C. § 1201(f) (reverse engineering for
  interoperability); judicial precedent in *Sega Enterprises Ltd. v.
  Accolade, Inc.* (9th Cir. 1992) and subsequent cases
- **Other jurisdictions:** equivalent provisions in most major
  copyright frameworks worldwide

## Trademarks

"Rackbus", "Commuwin", "FieldCare", "DeviceCare", "Commubox",
"Promass", "Promag", "Commutec", "Endress+Hauser", and similar marks
referenced in this documentation are or may be trademarks or
registered trademarks of their respective owners. Use of these terms
in this documentation is purely descriptive and nominal — to identify
the equipment with which this software is designed to interoperate.
No trademark license is granted or implied.

## Read-only by design

This implementation is deliberately limited to **read-only
operations**. Write functionality (parameter configuration, output
assignment, totalizer reset, etc.) is not provided in the public
release, to prevent accidental misconfiguration of production
equipment.

## No warranty

This software is provided "as is" without warranty of any kind. See
the [LICENSE](LICENSE) file. Users are responsible for verifying
correctness for their use case, especially in safety-critical or
compliance-sensitive contexts.

## Contact

If you are a representative of a manufacturer whose equipment is
discussed in this project and you have specific concerns about the
content or scope of this implementation, please open an issue or
contact the maintainers privately. We aim to be cooperative and will
address legitimate concerns promptly.
