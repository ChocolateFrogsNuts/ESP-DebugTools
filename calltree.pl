#!/usr/bin/perl

# Analyse the disassembled boot rom from the esp8266 looking for calls to 
# esp_intr_(un)lock, and functions that result in calls to them.
# Can be easily modified to work for other functions.

# Requires the git version of esp8266/Arduino as we use the nm tool.

my %tree;
my $irq_locked=0;

# 1=track if a call is made while lock is on - not completely reliable
# 0=assume lock is always on if func uses locking
my $track_lock_context=1;

my $a0;

open($boot, "<", "boot.txt");

while (<$boot>) {
   my $line = $_;
   chomp($line);
   if ($line =~ /^;/ ) {
      # comment
   } elsif ($line =~ /^([0-9|a-f]{8}):\s+?[0-9|a-f]{2,10}\s+?call0\s(.+?)\s/ ) {
      # call
      my $target = $2;
      #printf("Found call to '%s' in '%s'\n", $target, $funcname);

      if (!($target ~~ @{$tree{$funcname}{'calls'}})) {
         push(@{$tree{$funcname}{'calls'}}, $target);
      }
      if (!($funcname ~~ @{$tree{$target}{'calledby'}})) {
         push(@{$tree{$target}{'calledby'}}, $funcname);
      }
      
      if ($target eq 'ets_intr_lock') {
         $irq_locked++;
         $tree{$funcname}{'does_lock'}=1;
      } elsif ($target eq 'ets_intr_unlock') {
         $irq_locked-- if $track_lock_context;
         $tree{$funcname}{'does_unlock'}=1;
         $tree{$funcname}{'extra_unlocks'}=1 if $irq_locked<0;
      } elsif ($irq_locked) {
         $tree{$target}{'called_from_locked'}=1;
         $tree{$funcname}{'calls_while_locked'}=1;
      }
      
   } elsif ($line =~ /^([0-9|a-f]{8}):\s+?[0-9|a-f]{2,10}\s+?callx0\s(.+)/ ) {
      # indirect call
      # all we can do is flag it for manual checking
      my $reg = $2;
      $tree{$funcname}{'indirect_call'}=1;
      #printf("Found indirect call to [%s]\n", $reg);

   } elsif ($line =~ /^([0-9|a-f]{8}):\s+?[0-9|a-f]{2,10}\s+?l32i\s(.+?),(.+)/ ) {
      my $reg;
      my $src;
      $reg = $2;
      $src = $3;
      if ($reg eq 'a0') { $a0 = $src; }
      
   } elsif ($line =~ /^  (.+):$/ ) {
      # func name
      $funcname = $1;
      #printf("found %s\n", $funcname);

      # we don't analyse program flow, so we must assume unlocked at the 
      # start of each func, then flag any functions called while there 
      # might be a lock on.
      # visual analysis of the tree will tell us if there are calls
      # that might have nested locks.
      $irq_locked=0;
   }
}

close($boot);

# mark funcs called from SDK libs we use
foreach my $l (qw/airkiss crypto espnow main net80211 phy pp smartconfig wpa wpa2 wps/) {
   load_lib("lib".$l.".a", 'NONOSDK22y/', '');
}
# mark funcs used by non-SDK libs we use
foreach my $l (qw/bearssl gcc hal lwip2-1460-feat stdc++/) {
   load_lib("lib".$l.".a", '', '');
}

# mark funcs used by other SDK libs that we don't use
foreach my $l (qw/axtls/) {
   load_lib("lib".$l.".a", '', '*');
}

my %unique_funcs;
foreach my $f (qw/ets_intr_lock ets_intr_unlock/) {
   print_callers("", $f);
}

my @unique_funcs = keys %unique_funcs;
printf "%d functions involved:\n", $#unique_funcs+1;
foreach my $f (sort @unique_funcs) {
   my $refs=join(',',@{$tree{$f}{'used_by'}});
   printf  "  %s  %-27s  %s\n", func_flags($f), $f, $refs;
}

print "\n^=calls lock, v=calls unlock, >=calls a func that eventually calls lock/unlock\n";
print "!=extra calls to unlock, <=called from a func that uses lock/unlock";
print " while locked" if $track_lock_context;
print "\n";
print "* = Calls a func that results in locking, and does locking itself (> && (^ || v))\n";
print "\@ = Called by a func that is called from the SDK\n";
print ". = Not called by another ROM function\n";
print "I = makes indirect calls (to an address in a register)\n";

###

sub has_sdk_root
{ my ($func) = @_;

  return 1 if $#{$tree{$func}{'used_by'}} >=0;
  my $rc=0;
  foreach my $f (@{$tree{$func}{'calledby'}}) {
    $rc=1 if has_sdk_root($f);
  }
  return $rc;
}

sub has_callers
{ my ($func)=@_;
  return $#{$tree{$func}{'calledby'}} >= 0;
}

sub has_locking_leaf
{ my ($func, $lev) = @_;
  my $rc=0;
  $lev=0 unless defined($lev);
  foreach my $f (@{$tree{$func}{'calls'}}) {
     if ($f eq 'ets_intr_lock' || $f eq 'ets_intr_unlock') {
        return 1 if $lev>0;
     } else {
        $rc=1 if has_locking_leaf($f, $lev+1);
     }
  }
  return $rc;
}

sub func_flags
{ my ($func) = @_;
  my $f="";
  $f .= $tree{$func}{'called_from_locked'} ? '<' : ' ';
  $f .= $tree{$func}{'does_lock'}     ? '^' : ' ';
  $f .= $tree{$func}{'does_unlock'}   ? 'v' : ' ';
  $f .= $tree{$func}{'extra_unlocks'} ? '!' : ' ';
  $f .= has_locking_leaf($func)       ? '>' : ' ';
  $f .= ' ';
  $f .= has_locking_leaf($func) && ($tree{$func}{'does_lock'} || $tree{$func}{'does_unlock'})
            ? '*' : ' ';
  $f .= $tree{$func}{'calls_while_locked'} ? '+' : ' ';
  $f .= has_sdk_root($func)           ? '@' : ' ';
  $f .= has_callers($func)            ? ' ' : '.';
  $f .= $tree{$func}{'indirect_call'} ? 'I' : ' ';
  return $f;
}

sub print_callers
{ my ($prefix, $func)=@_;

  printf("%s%s  %s\n", $prefix, $func, func_flags($func));
  $unique_funcs{$func}=1;
  foreach my $f (@{$tree{$func}{'calledby'}}) {
     print_callers($prefix."    ", $f);
  }

}

sub load_lib
{ my ($fn,$sdkver,$marker)=@_;
  my $fh;
  my $obj;
  my $tools=$ENV{'HOME'}."/Arduino/hardware/esp8266com/esp8266/tools/";
  my $libfile=$tools."sdk/lib/".$sdkver.$fn;
  
  if (! -f $libfile) {
     die("Can't find ".$libfile);
     }
    
  open($fh, "-|", $tools."xtensa-lx106-elf/bin/xtensa-lx106-elf-nm ".$libfile)
     || die("Unable to run 'nm ".$libfile."'");

  while (<$fh>) {
    my $line = $_;
    if ($line =~ /^([0-9|a-f]{8}) (.) (.+)/ ) {
       # item is in this archive
       my $addr = $1;
       my $type = $2;
       my $name = $3;
       my $ref  = $fn.":".$obj.$marker;
       
       #if (defined($tree{$name})) {
       #   if (!($ref ~~ @{$tree{$name}{'used_by'}})) {
       #      push @{$tree{$name}{'used_by'}}, $ref;
       #   }
       #}
    }
    elsif ($line =~ /^ {8} (.) (.+)/ ) {
       # Referenced by but not in this archive
       my $type = $1;
       my $name = $2;
       my $ref  = $fn.":".$obj.$marker;
       
       if (defined($tree{$name})) {
          if (!($ref ~~ @{$tree{$name}{'used_by'}})) {
             push @{$tree{$name}{'used_by'}}, $ref;
          }
       }
    }
    elsif ($line =~ /^(.+?):$/ ) {
       # object file
       $obj = $1;
    }
  }
  
  close($fh);
}
