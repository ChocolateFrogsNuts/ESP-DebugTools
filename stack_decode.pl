#!/usr/bin/perl

# Stack dump decoder for ESP8266/Arduino.
# Requires the git version as we use objdump

# Assume latest build dir is current...
my $build_dir=`/bin/ls -1 -t -d /tmp/arduino_build_*|head -1l`;
my $sdk=$ENV{'HOME'}."/Arduino/hardware/esp8266com/esp8266";
chomp($build_dir);


my $elf = $build_dir."/DebugTools.ino.elf";
my $objdump = $sdk."/tools/xtensa-lx106-elf/bin/xtensa-lx106-elf-objdump";
my $linkscript = $sdk."/tools/esptool/flasher_stub/rom_8266.ld";

# first load the objdump of the elf....
my $obj = load_elf($elf);
# and the linker file for the boot rom addresses
my $rom = load_ld($linkscript);
# now the disassembly of the boot rom so we can extract the stack frame sizes
#read_dis("boot.txt");


my %ExceptionName=(
     0=>"Illegal Instruction",
     3=>"LoadStoreError",
     4=>"Level1 Interrupt",
     6=>"Division by Zero",
     9=>"Load/Store Alignment",
     28=>"Load Prohibited",
     29=>"Store Prohibited",
     );


my %stack;
my $instack=0;
my $stack_ptr=0;
my $exception;

print("\n");

while (<STDIN>) {
  my $line = $_;
  if ($line =~ />>>stack>>>/) {
     $instack=1;
     print("found >>>stack>>>\n");
     }
  elsif ($line =~ /<<<stack<<</) {
     $instack=0;
     print("found <<<stack<<<\n");
     }
  elsif ($line =~ /Dumping the umm_heap/) {
     $inheap=1;
     print("found heap\n");
     }
  elsif ($inheap && $line =~ /Total Entries/) {
     $inheap=0;
     print("got all of heap\n");
     }
  elsif ($instack>0) {
     if ($line =~ /([\d|a-f]{8}):  ([\d|a-f]{8}) ([\d|a-f]{8}) ([\d|a-f]{8}) ([\d|a-f]{8})/) {
        #printf("Data Line, addr=%08x\n", hex($1));
        my $sp = hex($1);
        $stack{$sp+0} =hex($2);
        $stack{$sp+4} =hex($3);
        $stack{$sp+8} =hex($4);
        $stack{$sp+12}=hex($5);
        
        $stack_ptr = $sp unless $stack_ptr;
        $stack_end = $sp+16;
        }
     elsif ($line =~ /ctx: (.+)/ ) {
        $stack_context=$1;
        chomp($stack_context);
        }
     }
  elsif ($inheap>0) {
     if ($line =~ /\|0x([\d|a-f]{8})\|(.+?)\|/) {
        my $addr=$1;
        my $block=$2;
        if ($addr >= $stack_ptr && $addr <= $stack_end) {
           printf("**** Heap block in stack area! ****  %s|%s\n", $addr, $block);
           }
        }
     }
  elsif ($line =~ /Exception \((\d+?)\):/ ) {
     $exception->{'reason'}=$1;
     printf("Exception %d: %s\n", $excep, $ExceptionName{$exception->{'reason'}});
     }
  elsif ($line =~ /Reset info: reason=(\d+?) / ) {
     $exception->{'reason'}=$1;
     printf("Exception %d: %s\n", $excep, $ExceptionName{$exception->{'reason'}});
     }
  elsif ($line =~ /epc1=(.{8,10}),{0,1} epc2=/) {
     my @line = split(/ /,$line);
     foreach my $item (@line) {
        my ($var,$val)=split(/=/,$item,2);
        $exception->{$var} = hex($val);
        }
     }
  }


printf("%s Stack Ptr: %08x  Stack End: %08x   %d (0x%x) bytes\n",
     $stack_context,
     $stack_ptr, $stack_end, 
     $stack_end - $stack_ptr,
     $stack_end - $stack_ptr);

if ($stack_ptr < ($stack_end-4096)) {
   printf("WARNING: stack is > 4K\n");
   if ($stack_ptr < ($stack_end - 16384)) {
      printf("WARNING: stack is > 16K which may run into dram!\n");
   }
}  

validate_stack_refs();
print("\n");
print_exception_info();
     
my $sp = $stack_ptr;
if ($ARGV[0]) {
   $sp = hex($ARGV[0]);
   printf("Starting with sp=%08x\n", $sp);
   print("\n");
   traverse_back($sp);
   print("\n");
   traverse_forward($sp);
} else {
   print("\n");
   traverse_back($stack_ptr);
   print("\n");
   traverse_forward($stack_end-4);
}

print("\n");
print_exception_info();


sub print_exception_info
{
  if (defined($exception) && defined($exception->{'reason'})) {
     printf("%s Exception (%d) at epc1=%08x  vector=%x",
         $ExceptionName{$exception->{'reason'}},
         $exception->{'reason'}, 
         $exception->{'epc1'},
         $exception->{'excvaddr'});
  
     my $ip=$exception->{'epc1'};
     my ($fname, $faddr, $fsize) = getAddrInfo($ip);
     if ($fname) {
        printf("<%s+%02x>\n", $fname, $ip - $faddr);
     } else {
        printf(" - invalid instruction address\n", $ip);
        my  $i=0;
        if (isOurCode($ip)) {
           while (($i<8) && !defined($obj->{$ip})) {
              $ip++;
           }
           if (defined($obj->{$ip})) {
              $fname = $obj->{$ip};
              printf("                 - address is at <%s+%02x>\n", 
                     $fname, $ip - $obj->{$fname}{'addr'});
           }
        }
     }

  }
}

sub validate_stack_refs
{
   # scan the stack for any addresses within the stack that refer to a location 
   # lower than where they are stored.

   my $sp = $stack_ptr;
   while ($sp < $stack_end) {
     if (isStack($stack{$sp}) && ($stack{$sp} < $sp)) {
        printf("WARNING: address on stack refers to part of stack that did not exist at the time\n");
        printf("     \@ %08x -> %08x   (%d bytes below self)\n", 
               $sp, $stack{$sp}, $sp - $stack{$sp});
     }
     $sp++;
   }   
}

sub traverse_back
{ my ($sp) = @_;
  my ($func, $faddr, $fsize, $ip);

  # Try traversing the stack from the current SP towards the end....
  print("Looking for a code ref:");
  while (($sp < $stack_end) && 
         (!isOurCode($stack{$sp}) || !defined($obj->{$stack{$sp}}))
        ) {
           # find a code ref because we don't know how big the top frame is
           $sp+=4;
  }

  printf(" sp=%08x\n", $sp);

  if ($sp < $stack_end) {
    my $ip = $stack{$sp};
    ($func, $faddr, $fsize) = getAddrInfo($ip);
    
    printf("Top Frame:\n");
    printf("\@%08x    returns to %08x <%s+%02x> whose frame is %s\n", 
        $sp, $ip, $func, $ip - $faddr, 
        defined($fsize) ? $fsize." bytes" : "unknown");
    $sp += 4;
  }

  # now we know the frame size of the next frame, we can traverse the stack
  my $count=0;
  while (($sp < $stack_end) && ($count<5) && ($fsize>0)) {
    my $bp = $sp + $fsize - 4;
    my $ip = $stack{$bp};

    $sp += $fsize;
    
    ($func, $faddr, $fsize) = getAddrInfo($ip);

    printf("\@%08x    returns to %08x <%s+%02x> %s\n", 
        $bp, $ip, $func, $ip - $faddr, 
        defined($fsize) ? "(".$fsize." byte frame)" : "(unknown frame size)");
        
    if (!isOurCode($ip) && !isRomCode($sp)) {
       printf("ERROR: caller address is not a code ref!\n");
    } else {
       $count=0;
    }
    $count++;
    
    if ($fsize<=0) {
       printf("The return address is invalid!\n");
       $count=9999; # quit
    }
  }
}

sub traverse_forward
{ my ($sp) = @_;
  my ($func, $faddr, $fsize, $ip);
  
  # Try traversing the stack from the end towards the current SP
  print("Looking for a code ref:");
  while (($sp >= $stack_ptr) && 
         (!isOurCode($stack{$sp}) || !defined($obj->{$stack{$sp}}))
        ) {
           # find a code ref because we don't know how big the top frame is
           $sp-=4;
  }

  printf(" sp=%08x\n", $sp);

  if ($sp > $stack_ptr) {
    my $ip = $stack{$sp};
    ($func, $faddr, $fsize) = getAddrInfo($ip);
    
    printf("Bottom Frame:\n");
    printf("    returns to %08x <%s+%02x> whose frame is %s\n", 
        $ip, $func, $ip - $faddr, 
        defined($fsize) ? $fsize." bytes" : "unknown");
    $sp += 4;
  }

  my $count=0;
  my $newsp=$sp-8;
  while (($newsp > $stack_ptr) && ($count<50)) {
    ($func, $faddr, $fsize) = getAddrInfo($stack{$newsp});

    if (isRomCode($stack{$newsp})) { 
       printf("%08x: ROM call detected (%08x: <%s+%02x>), moving sp to %08x\n", 
           $newsp, $stack{$newsp}, $func, $stack{$newsp}-$faddr, $newsp+4);
       $sp=$newsp+4;
       }
    elsif (isOurCode($stack{$newsp}) && defined($func)) {
       my $bp=$sp-4;
       # a valid code address, see if it's in the right place...
       $retip = $stack{$newsp};
       
       # is the next return addr where we would expect it to be?
       # - look at the end of this stack frame

       my $from = $stack{$bp};
       my ($from_func, $from_faddr, $from_fsize) = getAddrInfo($from);
       
       #printf("retip=%08x, sp=%08x newsp=%08x diff=%d, fsize=%d  func=%s\n", 
       #    $retip, $sp, $newsp, $sp-$newsp-4, $fsize, $func);
       
       if (($sp - $newsp - 4) == $fsize) {
          # yes - valid frame so print it.
          printf("%08x:    call from %08x <%s+%02x> %s to %08x <%s>\n",
                 $newsp, $from, $from_func, $from - $from_faddr, 
                 defined($fsize) ? "(".$fsize." byte frame)" : "(unknown frame size)", 
                 $retip, $func);
          $count=0;
          $sp=$newsp+4;
       } else {
          #printf("STACK CORRUPTED: return address found but not in the right place\n");
          #printf("                 sp-newsp=%d but fsize=%d\n", $sp-$newsp, $fsize);
          
          # attempt to re-sync
          
          printf("%08x: Trying to re-sync... %08x <%s+%02x>\n", 
                $newsp, $retip, $func, $retip-$faddr);
          $sp = $newsp+4;
          $count++;
       }
    }
    $newsp-=4;
  }  
}


sub isIram
{ my ($a)=@_;
  return 1 if ($a>=0x4010E000) && ($a<(0x4010E000+0x2000));
}

sub isDram
{ my ($a)=@_;
  return 1 if ($a>=0x3FFE8000) && ($a<(0x3FFE8000+0x14000));
}

sub isRomCode
{ my ($a)=@_;
  $a &= 0xFFF00000;
  return 1 if $a == 0x40000000;
  return 0;
}


sub isOurCode
{ my ($a)=@_;
  $a &= 0xFFF00000;
  return 1 if $a == 0x40100000;
  return 1 if $a == 0x40200000;
  return 0;
}

sub isStack
{ my ($a) = @_;
  return 1 if ($a>=$stack_ptr) && ($a<$stack_end);
  return 0;
}

sub isStackInDram
{ 
  return isDram($stack_ptr) && isDram($stack_end);
}


sub getAddrInfo
{ my ($addr)=@_;
  my ($func, $faddr, $fsize);

  if (isRomCode($addr)) {
       my $rec = getRomName($rom, $addr);
       if ($rec) {
          $func = $rec->{'name'};
          $faddr= $rec->{'addr'};
          $fsize=4;
          # actually we have no idea of frame size, and can't find out
          # without downloading the ROM and objdumping it, then
          # looking for the stack frame setup code on each function.
       }
  } elsif (isOurCode($addr)) {
       $func = $obj->{$addr};
       $faddr= $obj->{$func}{'addr'};
       $fsize= $obj->{$func}{'frame'};
  }

  return ($func, $faddr, $fsize);
}


sub load_elf
{ my ($fn) = @_;
  my $addr;
  my $name;
  my $frame;
  my %memmap;
  my $func_line=0;
  
  my $fh;
  $fn = $objdump.' -C -w -d '.$fn;
  printf("Reading %s\n", $fn);
  open($fh, '-|', $fn) || die($!);
  
  while (<$fh>) {
     my $line = $_;
     #printf("OBJ:%s\n", $line);
     if ($line =~ /^([\d|a-f]{8}) <(.+?)>:/) {
        $addr=hex($1);
        $name=$2;
        $memmap{$addr} = $name;
        $memmap{$name}{'frame'} = undef;
        $memmap{$name}{'addr'} = $addr;
        $func_line=0;
        #printf("Found %08x: %s\n", $addr, $name);
        }
     elsif ($line =~ /^([\d|a-f]{8}):\s+?.+?\s+?addi\s+?a1, a1, -(\d{1,4})/) {
        $addr=hex($1);
        $frame=$2;
        if ($func_line < 5) {
           $memmap{$name}{'frame'} = $frame;
           $func_line++;
           $memmap{$addr} = $name;
           #printf("Frame %s: %d (func_line=%d)\n", $name, $memmap{$name}, $func_line);
           }
        }
     elsif ($line =~ /^([\d|a-f]{8}):/) {
        $addr = hex($1);
        $memmap{$addr} = $name;
        #printf("ADDR: %08x %s\n", $addr, $name);
        }
    }
  close($fh);
  return \%memmap;
}

sub getRomName
{ my ($sorted_memmap, $addr)=@_;
  return undef unless isRomCode($addr);

  foreach my $r (@{$sorted_memmap}) {
    #printf("Check %s @ %08x <= %08x\n", $r->{'name'}, $r->{'addr'}, $addr);
    return $r if $r->{'addr'} <= $addr;
  }
  return undef;
}

sub load_ld
{ my ($fn) = @_;
  my $fh;
  my @memmap;
  
  printf("Reading %s\n", $fn);
  open($fh, '<', $fn) || die($!);
  
  while (<$fh>) {
     my $line = $_;
     if ($line =~ /^PROVIDE \( (.+?) = 0x([\d|a-f]{8}) \)/ ) {
        my $rec;
        $rec->{'name'}=$1;
        $rec->{'addr'}=hex($2);
        #printf("Found %s @ %08x\n", $rec->{'name'}, $rec->{'addr'});
        push(@memmap, $rec) if isRomCode($rec->{'addr'});
     }
  }
  close($fh);
  
  # we will need them sorted for searching...
  my @sorted = sort { $b->{'addr'} <=> $a->{'addr'} } @memmap;
  return \@sorted;
}
  
sub read_dis
{ my ($fn)=@_;
  my $fh;
  my $func,$addr,$fsize,$a6;
  my $funcaddr, $funcline=0;
  
  printf("Reading boot disassembly %s\n", $fn);
  open($fh, "<", $fn) || (print("Can't load disassembly\n") && return);
  
  while (<$fh>) {
    my $line = $_;
    if ($line =~ /^;/) {
       # comment - ignore
    } 
    elsif ($line =~ /^\s*?(.+?):$/ ) {
       $func = $1;
       $funcline=0;
    } 
    elsif ($line =~ /^([\d|a-f]):/ ) {
       $addr=hex($1);
       $funcline++;
       if (defined($rom->{$addr})) {    # is this in our list of ROM functions?
          $func     = $rom->{'addr'}{'name'};
          $funcaddr = $addr;
          $fsize    = 0;               # it exists, must have at least no frame.
          $funcline = 1;
       }
       
       if ($funcline < 5) {
          if ($line =~ /^([\d|a-f]{8}):\s+?.+?\s+?movi\s+?a6, (\d{1,4})/) {
             # prepping a6 to subtract from a1 ?
             $a6=$2;
          }       
          elsif ($line =~ /^([\d|a-f]{8}):\s+?.+?\s+?sub\s+?a1, a1, a6/) {
             # actual adjustment of a1 by a6
             $fsize=$a6;
          }
          elsif ($line =~ /^([\d|a-f]{8}):\s+?.+?\s+?addi\s+?a1, a1, -(\d{1,4})/) {
             # direct adjustment of a1
             $fsize=$2;
             printf("FOUND: %08x %s, %d\n", $addr, $func, $fsize);
          }
       }
    }
  }
  close($fh);
}
