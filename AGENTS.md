# Polyglot project guide

Read this file before changing the library. It records decisions from the design discussion. Current headers are incomplete; do not treat an empty class or method as a finished API.

## Change approval workflow

- For every request about library design or code, first present a concrete proposal in chat with a representative code example. Explain the API choice and any unresolved behavior.
- Do not edit library code, tests, or documentation in response to such a request until the user explicitly asks to apply the proposed change. A question such as "how should we do this?" is a request for a proposal, not permission to implement it.
- Once the user explicitly authorizes implementation, change only the agreed scope and report what was changed and verified.

## Purpose and constraints

- Polyglot is a C++23, header-only library for framing text and binary protocols. Its core must parse received frames and serialize outgoing frames. Protocol-specific layers, such as binary payloads and AT-style commands, may be built on top.
- A frame's field order, field kinds, codecs, and field widths are known at compile time. Marker byte values may be supplied when constructing the frame.
- Never use the heap. The application owns input, output, and marker buffers. Do not add hidden dynamic allocation, input accumulation, or transport ownership.
- Frame parsing and serialization must use the same frame schema. Keep transport I/O separate: the application decides how to receive and send bytes.
- Prefer small, explicit C++ constructs and readable compile-time checks. Introduce helpers only when they clarify repeated logic. Avoid extra framing mode enums, tag hierarchies, wrappers, or speculative abstractions.
- Match the existing code style. Make design changes incrementally and discuss unresolved behavior before locking it into the API.
- In code examples and edits, do not add cosmetic line breaks inside calls, lambda signatures, template arguments, or fold expressions. Keep an expression on one line when it is readable that way. For example, write `result = result.and_then([&](std::size_t cursor) -> std::expected<std::size_t, Error> {` without breaking after `and_then(`.

## Framing schema

- `schema::frame::Frame<Fields...>` is the ordered sequence of wire fields and holds their configuration in a tuple. Keep `Frame` as a named type.
- Supported fields are `Start<N>`, `Stop<N>`, `Delimiter<N>`, `LengthField<From, To, Size, Codec>`, `Data<Size>`, `Checksum<From, To, Size, Codec>`, and `CustomField<Id, Size, Codec>`. `From` and `To` are zero-based positions in the `Frame` type list and include both endpoints. Position zero refers to the first field. Marker fields hold a non-owning `ByteView` of their `N` bytes. The application supplies exactly `N` bytes and keeps them alive while the frame or parser uses them; marker values may be set at construction.
- A frame has exactly one `Data`, at most one of each other built-in field kind, and any number of `CustomField` entries with unique IDs. `Start`, when present, is first. `Delimiter`, when present, is last and separates complete frames; it never appears inside a frame. `Stop`, when present, ends the frame immediately before an optional `Delimiter`.
- `LengthField` may occur at any position before `Data`, with other fields between them. A checksum may occur at any otherwise valid position. Both range endpoints must exist and `From` must not follow `To`.
- `Data<Auto>` or `Data<>` needs a boundary: a length range covering `Data`, `Stop`, or `Delimiter`. `Data<Fixed<N>>` provides its own compile-time size, including the valid one-field frame `Frame<Data<Fixed<N>>>`. `Fixed<N>` is a configuration of `Data`, not another wire field.
- Length, checksum, and custom fields have codecs and byte widths. The framing layer determines each field's byte extent; its codec converts those bytes to or from a typed value. Text and binary representations must both be possible.
- `codec::BinaryLEInteger<T>` and `codec::BinaryBEInteger<T>` support signed and unsigned 8/16/32/64-bit values. A field may use any compile-time width from one byte through `sizeof(T)`; decoding sign-extends signed values, and encoding rejects values that do not fit. `codec::BinaryLEFloat<T>` and `codec::BinaryBEFloat<T>` support only IEEE 754 `float` (binary32, four bytes) and `double` (binary64, eight bytes). Other floating formats need separate codecs. Codecs may expose `accepts_size(size)` for configuration validation.
- The binary codecs explicitly select least-significant-byte-first (`LE`) or most-significant-byte-first (`BE`) input and output. The framing layer does not reorder bytes.
- `codec::ASCIIDecimal<T>`, `codec::ASCIIHexadecimal<T, Prefix, LetterCase, ZeroPadding>`, and `codec::ASCIIBinary<T>` support signed and unsigned integers. `ASCIIDecimal` places ASCII digits at the start of the field and writes `0x00` into any unused bytes; decoding accepts `0x00` only after the digits. Hexadecimal prefix policies are `None`, `Lower`, and `Upper`: `Lower` requires and writes `0x`, while `Upper` requires and writes `0X`. Hexadecimal letter case is independent of the prefix: `Lower` accepts lowercase digits and writes lowercase digits, `Upper` accepts uppercase digits and writes uppercase digits, and `Mixed` accepts both digit cases and writes lowercase digits. A signed hexadecimal value puts `-` before any prefix. With `ZeroPadding=true`, its digits fill exactly `2 * sizeof(T)` characters; padding follows the sign and prefix. With `ZeroPadding=false`, encoding uses the shortest digit sequence, including one digit for zero. Prefixed hexadecimal input is limited to 19 bytes, the maximum `-0x` plus 16 digits; longer input returns `InvalidFormat` even if leading zeroes make the numeric value fit. Hexadecimal encoding writes only the representation and may leave extra output bytes untouched; decoding must receive a view of the representation itself. It rejects an output span shorter than the required representation. Binary supports `None`, `Lower`, and `Upper` prefix policies. `codec::ASCIIFloat<T, Format, Precision>` supports `float` and `double`. `Format` defaults to `std::chars_format::general` and may also be `fixed`, `scientific`, or `hex`. `Precision` defaults to 6, matching the omitted precision of `printf` for decimal `f`, `e`, and `g`; it follows the chosen `std::to_chars` format and may round. `Precision` must be nonnegative; `-1` is not supported. The library also uses 6 as the default template precision for `hex`, although omitted precision in `printf %a` has a different rule. Encoding writes text at the start of the field, fills spare bytes with `0x00`, and rejects values that do not fit the supplied capacity. Decoding ignores `Format` and `Precision` and accepts decimal fixed or scientific notation, hexadecimal floating notation with or without `0x`, and infinity or NaN. Hexadecimal notation without `0x` uses a `p` exponent to distinguish it from decimal notation. Decoding accepts `0x00` only after the text and rejects values outside the type range. NaN decoding does not promise to preserve NaN payload bits.
- Marker bytes inside payloads and other protocol-specific ambiguities are the schema author's responsibility. Do not reject these layouts by a generic compile-time rule.
- Compile-time framing rules live in `include/polyglot/schema/framing_structure_check.hpp` as `check::validate_structure`. Codec width compatibility is checked in `include/polyglot/schema/framing_config_check.hpp` as `check::validate_config`. Consumers such as `Parser` invoke both during construction and expose the configuration result through `ok()` and `error()`.

## Data and protocol-specific layers

- The framing core treats `Data` as an opaque byte region. It determines its boundaries and exposes a view to the application or a protocol-specific layer; it does not interpret commands, parameters, or categories inside `Data`.
- A protocol-specific layer may parse and serialize `Data` using the application's buffers. The core must not depend on a particular payload grammar or a specific external library.
- `CustomField<Id, Codec>` describes a field in the frame outside `Data`; it is distinct from a field inside the payload. Its compile-time ID identifies the field to the application and does not change when fields are reordered. `From` and `To` still refer to zero-based positions, not IDs.
- Commands, categories, and parameters are outside the framing core. Discuss any protocol-specific layer separately before adding its types to this library.

## Parsing and writing direction

- `Parser` receives a view of the application's accumulated bytes and reports how many bytes were consumed, so the application can remove them. The intended delivery is one callback per fully validated frame, exposing `Data` and typed custom fields by ID. Its current implementation is only a stub; the callback and result signatures remain open.
- `Parser` reads application-owned input without modifying or copying it. It exposes `Data` as raw wire bytes; the application or a protocol-specific layer interprets those bytes.
- Add a separate writer/serializer using the same frame schema. It should accept payload bytes or allow the application to fill `Data` in a caller-owned mutable byte span, then report bytes written or an error. The application sends that span through its chosen transport. The exact API name and signature are not yet fixed.
- The writer must emit configured framing fields and compute length and checksum from their declared ranges. A field that precedes bytes it covers can be reserved in the output span and filled after those bytes have been written.
- Frame-field codecs decode from `std::span<const std::uint8_t>` into `std::expected<T, Error>` and encode from `T` into a caller-owned `std::span<std::uint8_t>`, returning `std::optional<Error>`. An empty optional means success. Keep byte slicing and field placement in the frame layer, not in individual codecs.
- Errors, partial input behavior, callback signatures, and the serialization API still need design work. Do not present a tentative signature as an established contract.

## Verification

The `polyglot` CMake target is an interface library, so building it alone does not instantiate templates. Add representative uses to `tests/compile_check.cpp` when changing template behavior, then build and run `polyglot_tests` with CMake/CTest.

Before reviewing a codec, reread its current header and tests. Inspect the branches and boundary values before running tests. Test the intended behavior, including failures, exact buffer sizes, and spare capacity; do not change expected results merely to make tests pass. Report any failing test that reveals an unresolved codec defect.

Current `ASCIIHexadecimal` header issue: it uses `std::bit_width` without directly including `<bit>`.
