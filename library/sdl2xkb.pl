#!/bin/perl
use warnings;

open file_in, "<bindings.h" or die;
open file_out, ">bindings_xkb.h" or die;

@capital = ("home", "end", "left", "right", "up", "down", "return", "escape");

while (<file_in>) {
    $_ =~ s/SDLK_/XKB_KEY_/g;
    ($nimi_k) = ($_ =~ /XKB_KEY_(\w*)/);
    if ($nimi_k and $nimi_k ne "") {
	$nimi_k = lc $nimi_k;
	$_ =~ s/XKB_KEY_\w*/XKB_KEY_$nimi_k/;
	$_ =~ s/KMOD_/WLR_MODIFIER_/g;
	$_ =~ s/XKB_KEY_return/XKB_KEY_Return/;
	($kp_k) = ($_ =~ /XKB_KEY_kp_(\w*)/);
	if ($kp_k and $kp_k ne "") {
	    $char_now = uc substr $kp_k, 0, 1;
	    $kp_k =~ s/^./$char_now/;
	    $_ =~ s/XKB_KEY_kp_\w*/XKB_KEY_KP_$kp_k/;
	}
	(my $key, my $mods) = ($_ =~ /XKB_KEY_([a-z]),(.*WLR_MODIFIER_SHIFT)/);
	if ($mods and $mods ne "") {
	    $key = uc $key;
	    $_ =~ s/(XKB_KEY_).(.*)/$1$key$2/;
	}
	foreach my $word_now (@capital) {
	    if ($_ =~ /XKB_KEY_$word_now/) {
		$char_now = uc substr $word_now, 0, 1;
		$copy_now = $word_now;
		$copy_now =~ s/^./$char_now/;
		$_ =~ s/XKB_KEY_\w*/XKB_KEY_$copy_now/;
		last;
	    }
	}
    }
    print file_out $_;
}

close file_in;
close file_out;
