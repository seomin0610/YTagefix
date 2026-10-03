# AgeFix

Fixes the sideloaded YouTube app crashing about 15 seconds after launch on **iOS 26.2 beta 1 (`23C5027f`)**.

## Symptoms

- YouTube quits about 15 seconds after launch. The crashes can come in streaks lasting days, likely depending on server-side config.
- Crash report:
  ```
  Exception Type:  EXC_BAD_ACCESS (SIGSEGV)
  Exception Subtype: KERN_INVALID_ADDRESS at 0x0000000000000004
  ```
- The crashing thread is `com.apple.root.user-initiated-qos.cooperative`. Its stack contains only the YouTube binary and `libswift_Concurrency.dylib`, with no tweak code.

## Cause

1. Shortly after launch, YouTube calls Apple's age range API, `AgeRangeService.shared.isEligibleForAgeFeatures` (`DeclaredAgeRange.framework`).
2. YouTube **weak-links** the framework. iOS 26.2 beta 1 does not have the `isEligibleForAgeFeatures` symbol, so dyld binds it to NULL.
3. The OS reports version 26.2, so the `#available(iOS 26.2, *)` check passes. YouTube then reads the context size (`[ptr+4]`) from the NULL async function pointer and crashes.

Symbols involved:

```
_$s16DeclaredAgeRange0bC7ServiceV013isEligibleForB8FeaturesSbvg     // getter
_$s16DeclaredAgeRange0bC7ServiceV013isEligibleForB8FeaturesSbvgTu   // async function pointer
```

## How it works

- Scans the `__got` (non-lazy symbol pointers) of every loaded image and fills only the slots for the two symbols above that are **NULL**.
- The slots get a Swift async stub that returns `false`, plus an async function pointer to that stub.
- If the symbol exists (e.g. on release iOS), the slot is not NULL and nothing is changed.
- Symbols are matched by name, not by address. The tweak keeps working across YouTube versions as long as the API name stays the same.
- Only depends on `libSystem`. CydiaSubstrate / ElleKit are not required.

## Build

Requires [Theos](https://theos.dev).

```sh
./build.sh
# Output: ./AgeFix.dylib  (uses ~/theos if THEOS is not set)
```

The Makefile sets `AgeFix_USE_MODULES = 0`. Without it, the Linux Theos toolchain (clang 13) fails to build modules against recent SDKs.

## Usage

Inject `AgeFix.dylib` into the YouTube IPA, then sideload it. Any common injection tool works (Sideloadly, cyan, Feather, etc.). It can be used together with other tweaks such as YTLite / YouTube Plus.

## Target

- iOS 26.2 beta 1 (`23C5027f`)
- Analyzed on YouTube 21.24.3 (YTPlus 5.2.2)
- Not needed on release iOS. It does nothing there.

## License

[MIT](LICENSE)
