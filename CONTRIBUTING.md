# Contribution Guide

 This document is work in progress and pull requests are welcome.


## Pull Request Titles

All titles should follow a streamlines format and be expressive. The reasoning is that a release notes are auto-generated from PR titles and should be nice to read. Currently the auto-generated release notes are hand edited to look more uniform, it would be better if we can streamline this.

### Examples

The following examples show some edits which are currently done manually:

| Before edit                                  | Streamlined                                                  |
| -------------------------------------------- | ------------------------------------------------------------ |
| FIX: some bug in _TZ3000_32111               | Fix battery reporting for Tuya motion sensor (_TZ3000_32111) |
| Fixed on/off command logging.                | Fix on/off command logging                                   |
| Adding support for Sony Motion sensor        | DDF for Sony motion sensor (S12_UH)                          |
| Add DDF to support sharp temp./humi. (SH500) | DDF for Sharp temperature and humidity sensor (SH500)        |
| DDF: Add support for Tuya clone              | DDF for Tuya contact sensor (_TZ5555_232332)                 |
| Update z67_jhsjdh.json                       | Fix Tuya Powerstrip unlock sequence                          |

The "DDF for" prefix for new DDF based devices might not be the most beautiful and maybe changed in future with better use of tags, perhaps to be replaced by "Support for".

There are also PR titles which can't be streamlined but for DDFs and bug fixes  which account for 80% of all pull requests we should do it.

### Notes on "some"

Pull requests with the word "some" in the title will likely be rejected automatically in future with request to change. It is a very weak word which tells the reader nothing and should be be replaced by what is actually changed.

### TAGS

The release notes generator uses tags to place PR titles in respective categories. The following tags are currently supported and can be assigned to pull requests:

| TAG                | When to use                                                  |
| ------------------ | ------------------------------------------------------------ |
| Fix                | Bugfixes and solved problems.                                |
| Enhancement        | New functionality  /  new generic item or sub-device descriptions / removed legacy C++ code. |
| Device Request     | For new DDFs.                                                |
| Device Improvement | Existing DDF is improved (can be combined with Fix tag).     |

