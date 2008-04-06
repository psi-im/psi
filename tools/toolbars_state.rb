#!/usr/bin/env ruby -wKU

require 'base64'
require 'iconv'

raise "Pass toolbars-state data as argument." if ARGV.length != 1
data = Base64.decode64(ARGV[0])

$toolBarStateMarker    = 0xfe
$toolBarStateMarkerEx  = 0xfc
$dockWidgetStateMarker = 0xfd

class MainWinState
  def initialize(data)
    @data = data
    @offset = 0
  end

  def unpack(format, byte_length)
    result = @data.unpack("@#{@offset}#{format}")
    @offset += byte_length
    return result
  end

  def readQString
    bytes = unpack("N", 4)[0]
    return "" if bytes == 0xffffffff
    len = bytes / 2
    Iconv.open('UTF-8', 'UCS-2BE') do |cd|
      result = cd.iconv(@data[@offset, bytes])
      @offset += bytes
      return result
    end
  end

  def toSigned(num)
    length = 32
    mid = 2**(length-1) 
    max_unsigned = 2**length 
    (num >= mid) ? num - max_unsigned : num
  end

  def unpackRect(geom0, geom1)
    floating = geom0 & 1
    x = y = w = h = 0

    if floating != 0
      geom0 >>= 1;

      x = (geom0 & 0x0000ffff) - 0x7FFF
      y = (geom1 & 0x0000ffff) - 0x7FFF

      geom0 >>= 16;
      geom1 >>= 16;

      w = geom0 & 0x0000ffff;
      h = geom1 & 0x0000ffff;
    end

    return "(x:#{x} y:#{y} w:#{w} h:#{h} floating:#{floating != 0 ? 'true' : 'false'})"
  end

  def toolBarAreaName(pos)
    case pos
      when 0: "LeftDock"
      when 1: "RightDock"
      when 2: "TopDock"
      when 3: "BottomDock"
    end
  end

  def toolBarAreaLayoutRestoreState(tmarker)
    lines = unpack("N", 4)[0]
    (0...lines).each do |j|
      pos = unpack("N", 4)[0]
      cnt = unpack("N", 4)[0]
      (0...cnt).each do |k|
        objectName = readQString
        shown      = unpack("C", 1)[0]
        item_pos   = unpack("N", 4)[0]
        item_size  = unpack("N", 4)[0]
        geom0      = unpack("N", 4)[0]
        rect       = ""

        if tmarker == $toolBarStateMarkerEx
          geom1 = unpack("N", 4)[0]
          rect = unpackRect(geom0, geom1)
        end
        puts "#{objectName}"
        puts "\tpos:       #{toolBarAreaName(pos)}"
        puts "\tshown:     #{shown != 0 ? 'true' : 'false'}"
        puts "\titem_pos:  #{item_pos}"
        puts "\titem_size: #{item_size}"
        puts "\trect:      #{rect}"
      end
    end
  end

  def dockAreaLayoutRestoreState
    puts "dockAreaLayoutRestoreState:"
    cnt = unpack("N", 4)[0]
    (0...cnt).each do |i|
      raise "TODO"
    end

    width, height = unpack("NN", 8)
    puts "\tcentralWidgetRect = #{width} x #{height}"

    (0...4).each do |i|
      puts "\tcornerData[#{i}] = #{unpack('N', 4)}"
    end
  end

  def restoreState
    marker, version = unpack("NN", 8)
    raise "Incompatible format: #{marker}, #{version}" if marker != 0xff or version != 0

    while true do
      marker = unpack("C", 1)[0]

      case marker
      when $toolBarStateMarker, $toolBarStateMarkerEx
        # puts "toolBarAreaLayoutRestoreState"
        toolBarAreaLayoutRestoreState(marker)
      when $dockWidgetStateMarker
        # puts "dockAreaLayoutRestoreState"
        dockAreaLayoutRestoreState
      end

      break if @offset >= @data.length
    end
  end
end

MainWinState.new(data).restoreState
