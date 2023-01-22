NOTE: This component has been converted from an embedded copy to a git
submodule of the upstream repository during the course of the CMake conversion.

(This was done to isolate the upstream repo from the CMake conversion - there is
an already existing `CMakeLists.txt` file in this repo that we cannot use as-is)

There were some OVMS-specific changes, that have been (temporarily) lost
during the conversion:
* https://github.com/openvehicles/Open-Vehicle-Monitoring-System-3/commit/04fafb83a3cc5563a5feb70a4bb7922457d58893#diff-68853826f98a9c857b394d3bc1b5821c8542054b2ddff59a90c260f4fe225bf4
  * Is present in wolfssl version to `v4.7.1r` so we switched to it
* https://github.com/openvehicles/Open-Vehicle-Monitoring-System-3/commit/51444539047daef7bd2accb23ef40d1bc14fdb20
  * Temporarily lost, we may need to fork the upstream repo to re-apply it
