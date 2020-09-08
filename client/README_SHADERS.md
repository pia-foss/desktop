# QtGraphicalEffect shader resources

Two directories provide variations of the QtGraphicalEffects shaders:

* shader_res_gl - OpenGL and OpenGL Core Profile shaders (from Qt 5.15.0 - https://code.qt.io/cgit/qt/qtgraphicaleffects.git/tree/src/effects/shaders?h=5.15.0)
* shader_res_rhi - RHI QShader archives (from Qt 6.0-dev - https://code.qt.io/cgit/qt/qtgraphicaleffects.git/tree/src/effects/shaders_ng)

PIA Desktop uses the direct OpenGL backend for Qt Quick on Mac and Linux.  However, for whatever reason, the QtGraphicalEffects shaders do not seem to be present in the QtGraphicalEffects shared libraries - this seems to be a recurrent issue with the QRC resource system in libraries - so the shaders are included in the PIA client resources.

On Windows, PIA Desktop uses the RHI D3D11 backend.  QtGraphicalEffects in 5.15.0 does not ship QShader archives containing shaders for RHI backends.  However, the qsb shader archives from 6.0-dev are compatible, and we can drop them in place of the GLSL shaders (omitting the .qsb extension) so the 5.15.0 QtGraphicalEffects will load them.  From there, the RHI backend knows what to do with those shaders.

In all cases, only the shaders actually used by PIA are present.  Due to the various backend limitations, dynamically-generated shader effects can't be used at all - there's no way to provide RHI shaders.  (This includes DropShadow and GaussianBlur - but a shadow can be approximated similarly with a FastBlur and LevelAdjust, as in WhatsNewContent.qml.)
