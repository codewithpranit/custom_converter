# Custom Converter

A high-performance encoding converter for UTF-8, ISCII, and Acharya formats, strictly adhering to the large-code-base repository logic.

## Installation

You can install the package locally for development:

```bash
cd custom_converter
pip install -e .
```

## Usage

After installation, you can run the interactive CLI from anywhere:

```bash
custom-converter
```

The CLI will guide you through:
1.  Selecting a conversion function.
2.  Providing either a file path (for text inputs) or a hex string (for raw byte/syllable inputs).

### Functions supported:
- `pipeline`: Full verification flow from UTF-8 to Acharya and back.
- `utf8_to_acharya`: Convert text file to Acharya syllables.
- `acharya_to_utf8`: Convert Acharya hex string to UTF-8.
- `acharya_to_iscii`: Convert Acharya hex string to ISCII bytes.
- `iscii_to_acharya`: Convert ISCII hex string to Acharya syllables.
- `utf8_to_iscii`: Convert text file to ISCII bytes.
- `iscii_to_utf8`: Convert ISCII hex string to UTF-8.

## Hex Input Format
Hex strings can be provided with or without the `0x` prefix. For Acharya syllables, bytes should be space-separated (e.g., `0xFA 0x01 B4 00`).
