# QemuVideoDxe

Since the upstream edk2 removed ARM support from QemuVideoDxe
(in commit cefbbb3d087143316fba077dd02964afb92f647f), I've
decided to duplicate the driver here in a convenient form
that still builds for ARM and AARCH64.

I love how breaking TCG qemu is fine because of a KVM issue.

That totally makes sense. Right? Oh...

This reverts the changes done as part of edk2 commit
bfb0ee0adbb1838a371e9f5b1c1b49e1f354447a.

Other changes:
- Remove dependency on TimerLib (not used).