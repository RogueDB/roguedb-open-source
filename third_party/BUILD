load("@rules_uv//uv:pip.bzl", "pip_compile")

pip_compile(
    name = "requirements",
    requirements_in = "requirements.in",  # default
    requirements_txt = "requirements.txt",
    visibility = ["//:__subpackages__"],
)
