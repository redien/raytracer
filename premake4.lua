
-- raytracer - A real-time raytracer.
-- Written in 2014 by Jesper Oskarsson jesosk@gmail.com
--
-- To the extent possible under law, the author(s) have dedicated all copyright
-- and related and neighboring rights to this software to the public domain worldwide.
-- This software is distributed without any warranty.
--
-- You should have received a copy of the CC0 Public Domain Dedication along with this software.
-- If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.

solution "RaytracingDemo"
	configurations {"Debug", "Release"}
	location "build"

	includedirs {
		"../limbus/include",
		"../limbus/generated/cpp",
		"../pingo/include",
		"../pingo/generated/cpp",
		"../glm",
		"./include",
	}

    buildoptions "-std=c++11 -stdlib=libc++"
    linkoptions "-stdlib=libc++"

	project "raytracing"
		language "C++"
		location "build"
		targetdir "./bin"

		files {"source/main.cpp"}

		links {
			"limbus",
			"limbus_cpp",
			"pingo",
			"pingo_cpp"
		}


        if os.is "windows" then
            links {
                "opengl32"
            }
        end

        if os.is "macosx" then
            links {
                "OpenGL.framework",
                "ForceFeedback.framework",
                "Cocoa.framework",
                "Carbon.framework",
                "IOKit.framework",
                "CoreAudio.framework",
                "AudioToolbox.framework",
                "AudioUnit.framework",
                "SDL2",
                "m"
            }
        end

		libdirs {
			"../limbus/lib",
			"../pingo/lib",
		}

		configuration "Debug"
			includedirs {
				"../boost_1_55_0/",
			}
			libdirs {
				"../boost_1_55_0/stage/lib"
			}
			flags {"Symbols", "ExtraWarnings"}
			kind "ConsoleApp"
			defines {"DEBUG"}
		
		configuration "Release"
			includedirs {
				"../boost_1_55_0/",
			}
			libdirs {
				"../boost_1_55_0/stage/lib"
			}
			flags {"OptimizeSpeed", "StaticRuntime"}
			kind "WindowedApp"
			--kind "ConsoleApp"
			defines {"NDEBUG"}
