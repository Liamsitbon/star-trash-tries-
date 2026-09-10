# Third-party notices

NE Fixed Add-on is an independent add-on, not an official Noodle Extensions release. The original NE binary is required as a separate dependency, not redistributed or renamed inside this QMOD. Adapted local classifier/compatibility code retains the MIT notices in `LICENSE`.

Public SDK/runtime dependencies are beatsaber-hook, bs-cordl, CustomJSONData, custom-types, SongCore, Lapiz, BSML, Scotland2, Paper and config-utils, with their transitive header dependencies (including RapidJSON, fmt and Sombrero). QPM restores them from their original packages; they retain their own licenses and copyright notices. Runtime dependency binaries are not copied into this QMOD. Android platform/IL2CPP method metadata is used to call the installed game; no game binary or map assets are included.

The ARM64 inline-hook implementation supplied by beatsaber-hook is compiled into the add-on. Its notice is reproduced below.

## And64InlineHook

MIT License

Copyright (c) 2018 Rprop (r_prop@outlook.com)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
