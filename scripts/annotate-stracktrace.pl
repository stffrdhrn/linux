#!/usr/bin/perl

# Usage annotate-stacktrace.pl <raw-trace> <System.map>
#
# Outputs the trace with symbols resolved from system.map
# The raw-trace should be in a format compatible with the
# show_registers() out put from arch/openrisc/kernel/traps.c
#
# Author: Stafford Horne <shorne@gmail.com>
#
use strict;

my $raw_trace  = $ARGV[0];
my $system_map = $ARGV[1];

if (!(-f $raw_trace) || !(-f $system_map)) {
  die("usage: annotate-stacktrace.pl <raw-trace> <System.map>");
}

sub parse_system_map {
  my ($system_map) = @_;

  my $sm_fh;
  my $sm = {};
  open ($sm_fh, $system_map) or die ("cannot open $system_map: $!");
  while (<$sm_fh>) {
    chomp;
    my ($addr, $type, $name) = split / /;
    $sm->{'0x'.$addr} = {
      addr => '0x'.$addr,
      type => uc $type,
      name => $name,
    };
  }
 
  return $sm;
}

sub resolve_symbol {
  my ($symbol_map, $addr) = @_;
  my $offset = 0;

  my $symbol = $symbol_map->{$addr};
  while (!$symbol && $offset < 9000) {
     $offset = $offset + 4;
     my $new_addr = sprintf "0x%08x", (hex($addr) - $offset);
     $symbol = $symbol_map->{$new_addr};
  }

  my $symbol_name = $symbol->{name};
  if ($symbol->{type} eq 'T') {
    $symbol_name .= '()';
  } elsif ($symbol_name) {
    $symbol_name = "\t$symbol_name";
  }
  if ($symbol_name && $offset != 0) {
    $symbol_name .= '+'.$offset;
    if ($symbol->{name} eq 'init_thread_union') { #this is a kernel stack pointer
      $symbol_name .= ' (stackdepth '. ((1<<13) - $offset) . ')';
    }
  }

  return $symbol_name;
}

##
# MAIN
##

my $symbols = parse_system_map($system_map);

my $stack_fh;
my $stack_top;   # stack pointer before crash
my $stack_addr;  # current stack address

open ($stack_fh, $raw_trace) or die ("cannot open $raw_trace: $!");

while (<$stack_fh>) {
  chomp;
  s/\s+$//;

  print $_;

  if (/Stack dump \[(0x.*)\]/) {
    $stack_addr = hex($1);
  } else {
    # openrisc kernel is mapped in 0xc0...... range
    # resolve symbols in that range
    if (/ (0x[a-f0-9]{8})$/) {
      # Annotate stack
      if ($stack_addr && !$stack_top) {
        # the first value on the stack is a pointer to the original SP
        $stack_top = $1;
      }
      my $symbol = resolve_symbol($symbols, $1);
      my $stack_pointer = sprintf "0x%08x", $stack_addr;
      print "\t".$stack_pointer."\t". $symbol;
      if ($stack_top eq $stack_pointer) {
        print "~~~~iCRASH TOP!~~~~";
      }
      $stack_addr = $stack_addr + 4;
    } elsif (/\[<(c0[a-f0-9]{6})>\]$/) {
      # Annotate call stack (at end of stack)
      my $symbol = resolve_symbol($symbols, '0x'.$1);
      print "\t". $symbol;
    } elsif (/GPR09: (c0[a-f0-9]{6})/) {
      # Annotate R9 return address
      my $symbol = resolve_symbol($symbols, '0x'.$1);
      print "\t R9 return ". $symbol;
    }
  }
  print "\n";
}
