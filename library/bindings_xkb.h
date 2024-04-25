#define ALT	WLR_MODIFIER_ALT
#define SHIFT	WLR_MODIFIER_SHIFT
#define CTRL_	WLR_MODIFIER_CTRL

/* In mousepaint_m mode these override the default bindings.
   Drawing with mouse does not affect the original file.
   A new file is created if changes are saved. */
static Binding keydown_bindings_mousepaint_m[] = {
    /* Select the value to write to memory when drawing with mouse.
       Alternatively, the filename of a shared object file (*.so) can be given.
       This should contain a function with the following prototype:
       void* function(void* in, void* out);
       $in is a pointer to the value under the mouse and
       $out is a pointer to the memory where the new value is to be written.
       The function should return $out.
       If the file is in the current folder, write ./file.so instead of file.so. */
    { XKB_KEY_space,	0,	mp_set_action,				},
    /* Save the current dataset with all changes as a new file.
       The default filename contains the Unix time. */
    { XKB_KEY_Return,	0,	mp_save,				},
    { XKB_KEY_KP_Enter,	0,	mp_save,				},
    /* Save only the current frame; not other variables or timesteps. */
    { XKB_KEY_Return,	SHIFT,	mp_save_frame,				},
    { XKB_KEY_KP_Enter,	SHIFT,	mp_save_frame,				},
    { XKB_KEY_s,	0,	set_typingmode,	{.i=typing_mp_filename}	},
    /* Make the pen bigger or smaller. */
    { XKB_KEY_plus,	0,	mp_size,	{.i=1}			},
    { XKB_KEY_minus,	0,	mp_size,	{.i=-1}			},
};

/* In variables_m mode these override the default bindings.
   In that mode, names of all variables are shown in the command line.
   The following keybindings are used to choose a variable to switch to. */
static Binding keydown_bindings_variables_m[] = {
    { XKB_KEY_Up,	0,		pending_var_dec, },
    { XKB_KEY_V,	ALT|SHIFT,	pending_var_dec, },
    { XKB_KEY_Down,	0,		pending_var_inc, },
    { XKB_KEY_v,	ALT,		pending_var_inc, },
    { XKB_KEY_Return,	0,		use_and_exit,    },
    { XKB_KEY_KP_Enter,	0,		use_and_exit,    },
};

/* In colormaps_m mode these override the default bindings.
   In that mode, names of all colormaps are shown in the command line.
   The following keybindings are used to choose a colormap to switch to. */
static Binding keydown_bindings_colormaps_m[] = {
    { XKB_KEY_Up,	0,		pending_map_dec,	},
    { XKB_KEY_C,	ALT|SHIFT,	pending_map_dec,	},
    { XKB_KEY_Down,	0,		pending_map_inc,	},
    { XKB_KEY_c,	ALT,		pending_map_inc,	},
    { XKB_KEY_Return,	0,		use_map_and_exit,	},
    { XKB_KEY_KP_Enter,	0,		use_map_and_exit,	},
};

static Binding keydown_bindings[] = {
    { XKB_KEY_q,	0,		quit,					},
    { XKB_KEY_h,	0,		show_bindings,				}, // show this file
    { XKB_KEY_v,	0,		var_ichange,	{.i=1}			}, // Next variable.
    { XKB_KEY_V,	SHIFT,		var_ichange,	{.i=-1}			}, // Previous variable.
    { XKB_KEY_v,	ALT,		set_prog_mode,	{.i=variables_m}	}, // see keydown_bindings_variables_m for info
    { XKB_KEY_w,	0,		use_lastvar,				}, // switch to that variable which was shown previously
    /* Whether globals such as colormap, invert_y, etc. are detached from other variables. */
    { XKB_KEY_d,	0,		toggle_detached,			},
    /* Colorscale adjustment.
       For example, if a few values are much higher than other, one may want to make the largest value smaller. */
    { XKB_KEY_1,	0,		shift_min,	{.f=-0.02}		}, // make the smallest value smaller
    { XKB_KEY_exclam,	SHIFT,		shift_max,	{.f=-0.02}		}, // make the largest value smaller
    { XKB_KEY_2,	0,		shift_min,	{.f=0.02}		}, // make the smallest value larger
    { XKB_KEY_quotedbl,	SHIFT,		shift_max,	{.f=0.02}		}, // make the largest value larger
    { XKB_KEY_0,	0,		toggle_var,	{.v=&update_minmax_cur}	}, // scale colors based on the current frame
    /* Shift the colorscale as above but different step size. */
    { XKB_KEY_1,	ALT,		shift_min_abs,	{.f=-0.02}		},
    { XKB_KEY_exclam,	SHIFT|ALT,	shift_max_abs,	{.f=-0.02}		},
    { XKB_KEY_2,		ALT,		shift_min_abs,	{.f=0.02}		},
    { XKB_KEY_quotedbl,	SHIFT|ALT,	shift_max_abs,	{.f=0.02}		},
    { XKB_KEY_k,	0,		toggle_threshold,			},
    /* Choosing the colormap. */
    { XKB_KEY_c,	0,		cmap_ichange,	{.i=1}			}, // Next colormap.
    { XKB_KEY_C,	SHIFT,		cmap_ichange,	{.i=-1}			}, // Previous colormap.
    { XKB_KEY_c,	ALT,		toggle_var,	{.v=&globs.invert_c}	}, // Invert (reverse) the colormap.
    { XKB_KEY_C,	SHIFT|ALT,	set_prog_mode,	{.i=colormaps_m}	}, // see keydown_bindings_colormaps_m
    /* Swap foreground and background colors. */
    { XKB_KEY_I,	SHIFT,		invert_colors,				},
    /* Print information about the dataset which is being plotted. */
    { XKB_KEY_p,	0,		print_var,				},
    /* Toggle whether iformation such as the current variable is printed into the terminal. */
    { XKB_KEY_e,	0,		toggle_var,	{.v=&globs.echo}	},
    /* If enabled, virtual pixels correspond exactly to data. This disables the chance for stepless zoom. */
    { XKB_KEY_E,	SHIFT,		toggle_var,	{.v=&globs.exact}	},
    /* Toggle whether the largest y-coordinate is in the top or in the bottom. */
    { XKB_KEY_i,	0,		toggle_var,	{.v=&globs.invert_y}	},
    /* Play a movie, if the variable has third dimension. Use fps to control the speed. */
    { XKB_KEY_space,	0,		toggle_play,				},
    { XKB_KEY_space,	SHIFT,		toggle_play_rev,			},
    { XKB_KEY_s,	0,		set_typingmode,	{.i=typing_fps}		},
    /* Jump to another frame, if data has third dimension. */
    { XKB_KEY_j,	0,		set_typingmode,	{.i=typing_goto},	},
    /* Show or hide coastlines.
       By default, the fastet changing coordinates are handled as longitudes
       and the second fastest changing as latitudes.
       For other options, see ask_crs. */
    { XKB_KEY_l,	0,		toggle_var,	{.v=&globs.coastlines}	},
    /* In mousepaint mode, the file can be edited by drawing to it with mouse.
       Further documentation and the keybindings are listed in keydown_bindings_mousepaint_m. */
    { XKB_KEY_m,	0,		set_prog_mode,	{.i=mousepaint_m}	},
    /* Set a value which will handled such as NAN values.
       The value which to use will be asked in terminal. */
    { XKB_KEY_n,	0,		set_typingmode,	{.i=typing_nan},	},
    /* Toggle whether there is a specific fill value which is handled such as NAN values. */
    { XKB_KEY_N,	SHIFT,		toggle_var,	{.v=&globs.usenan}	},
    /* Toggle whether the figure fills the whole window
       meaning that all data are not seen unless the aspect ratio is exactly right. */
    { XKB_KEY_f,	0,		toggle_var,	{.v=&fill_on}		},
    { XKB_KEY_plus,	0,		multiply_zoom,	{.f=0.85}		}, // smaller number is more zoom
    { XKB_KEY_minus,	0,		multiply_zoom,	{.f=1/0.85}		}, // larger number is less zoom
    { XKB_KEY_X,	SHIFT,		multiply_zoomx,	{.f=0.85}		},
    { XKB_KEY_x,	0,		multiply_zoomx,	{.f=1/0.85}		},
    { XKB_KEY_Y,	SHIFT,		multiply_zoomy,	{.f=0.85}		},
    { XKB_KEY_y,	0,		multiply_zoomy,	{.f=1/0.85}		},
    { XKB_KEY_Right,	0,		inc_znum,	{.i=1}			},
    { XKB_KEY_Left,	0,		inc_znum,	{.i=-1}			},
    /* Move the zoom/view rectangle 7 steps. */
    { XKB_KEY_Right,	ALT,		inc_offset_i,	{.i=7}			},
    { XKB_KEY_Left,	ALT,		inc_offset_i,	{.i=-7}			},
    { XKB_KEY_Up,	ALT,		inc_offset_j,	{.i=-7}			},
    { XKB_KEY_Down,	ALT,		inc_offset_j,	{.i=7}			},
    { XKB_KEY_Up,	0,		inc_offset_j,	{.i=-7}			},
    { XKB_KEY_Down,	0,		inc_offset_j,	{.i=7}			},
    /* Move the zoom/view rectangle 1 step. */
    { XKB_KEY_Right,	SHIFT|ALT,	inc_offset_i,	{.i=1}			},
    { XKB_KEY_Left,	SHIFT|ALT,	inc_offset_i,	{.i=-1}			},
    { XKB_KEY_Up,	SHIFT|ALT,	inc_offset_j,	{.i=-1}			},
    { XKB_KEY_Down,	SHIFT|ALT,	inc_offset_j,	{.i=1}			},
    { XKB_KEY_Return,	0,		use_pending,				},
    { XKB_KEY_KP_Enter,	0,		use_pending,				},
    { XKB_KEY_Escape,	0,		end_curses,				},
    /* Set the coordinate system of this variable.
       If proj-library is available, this will affect how the coastlines are drawn.
       Otherwise this is useless. */
    { XKB_KEY_T,	SHIFT,		set_typingmode,	{.i=typing_crs}		},
#ifdef HAVE_PNG
    { XKB_KEY_s,	CTRL_,		save_png,				},
#endif
#ifdef HAVE_NCTPROJ
    /* Convert this variable into different coordinates.
       The original and target coordinate systems will be asked in terminal.
       See e.g. 'man proj' for how to express the coordinate systems. */
    { XKB_KEY_t,	0,		set_typingmode,	{.i=typing_coord_from},	},
#endif
#ifdef HAVE_TTRA
    { XKB_KEY_P,	SHIFT,		toggle_var,	{.v=&use_ttra}		},
#endif
};

#undef ALT
#undef SHIFT
#undef CTRL_
