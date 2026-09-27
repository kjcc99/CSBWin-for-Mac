// Draws macos/CSBwin.icns artwork: the sword from CSBwin.ico, recolored, on a
// dark stone tile.  Usage: swift make_icon.swift <sword mask.txt> <out.png> <size>
import AppKit

let args = CommandLine.arguments
let mask = try! String(contentsOfFile: args[1], encoding: .utf8).split(separator: "\n").map { Array($0) }
let size = CGFloat(Int(args[3])!)

func rgb(_ hex: UInt32) -> CGColor {
   CGColor(red: CGFloat((hex >> 16) & 255) / 255, green: CGFloat((hex >> 8) & 255) / 255, blue: CGFloat(hex & 255) / 255, alpha: 1)
}

let rep = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: Int(size), pixelsHigh: Int(size), bitsPerSample: 8, samplesPerPixel: 4, hasAlpha: true, isPlanar: false, colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0)!
let ctx = NSGraphicsContext(bitmapImageRep: rep)!.cgContext
ctx.scaleBy(x: size / 1024, y: size / 1024)
// Flip so y grows downward like the mask rows.
ctx.translateBy(x: 0, y: 1024)
ctx.scaleBy(x: 1, y: -1)

// Tile on the macOS icon grid: 824pt square, 100pt margin.
let tile = CGRect(x: 100, y: 100, width: 824, height: 824)
let tilePath = CGPath(roundedRect: tile, cornerWidth: 185, cornerHeight: 185, transform: nil)
ctx.saveGState()
ctx.setShadow(offset: CGSize(width: 0, height: 12), blur: 28, color: CGColor(gray: 0, alpha: 0.5))
ctx.addPath(tilePath)
ctx.setFillColor(rgb(0x23262e))
ctx.fillPath()
ctx.restoreGState()

ctx.saveGState()
ctx.addPath(tilePath)
ctx.clip()
let gradient = CGGradient(colorsSpace: CGColorSpaceCreateDeviceRGB(), colors: [rgb(0x464b57), rgb(0x1b1d23)] as CFArray, locations: [0, 1])!
ctx.drawLinearGradient(gradient, start: CGPoint(x: 512, y: 100), end: CGPoint(x: 512, y: 924), options: [])
// Dungeon wall: staggered stone blocks.
ctx.setStrokeColor(CGColor(gray: 0, alpha: 0.28))
ctx.setLineWidth(6)
let rowHeight: CGFloat = 103, blockWidth: CGFloat = 206
for row in 0..<9 {
   let y = 100 + CGFloat(row) * rowHeight
   ctx.move(to: CGPoint(x: 100, y: y)); ctx.addLine(to: CGPoint(x: 924, y: y))
   var x = 100 + (row % 2 == 0 ? 0 : blockWidth / 2)
   while x < 924 {
      ctx.move(to: CGPoint(x: x, y: y)); ctx.addLine(to: CGPoint(x: x, y: y + rowHeight))
      x += blockWidth
   }
}
ctx.strokePath()
// Warm torchlight glow behind the sword.
let glow = CGGradient(colorsSpace: CGColorSpaceCreateDeviceRGB(), colors: [CGColor(red: 1, green: 0.75, blue: 0.35, alpha: 0.35), CGColor(red: 1, green: 0.6, blue: 0.2, alpha: 0)] as CFArray, locations: [0, 1])!
ctx.drawRadialGradient(glow, startCenter: CGPoint(x: 512, y: 512), startRadius: 0, endCenter: CGPoint(x: 512, y: 512), endRadius: 420, options: [])
// Inner highlight on the tile edge.
ctx.addPath(tilePath)
ctx.setStrokeColor(CGColor(gray: 1, alpha: 0.12))
ctx.setLineWidth(8)
ctx.strokePath()
ctx.restoreGState()

// The sword, one square per icon pixel.
func filled(_ x: Int, _ y: Int) -> Bool {
   y >= 0 && y < mask.count && x >= 0 && x < mask[y].count && mask[y][x] == "#"
}
var minX = 99, maxX = 0, minY = 99, maxY = 0
for y in 0..<mask.count { for x in 0..<mask[y].count where filled(x, y) {
   minX = min(minX, x); maxX = max(maxX, x); minY = min(minY, y); maxY = max(maxY, y) } }
let cell: CGFloat = 21
let originX = 512 - CGFloat(maxX - minX + 1) * cell / 2 - CGFloat(minX) * cell
let originY = 512 - CGFloat(maxY - minY + 1) * cell / 2 - CGFloat(minY) * cell
func square(_ x: Int, _ y: Int) -> CGRect {
   CGRect(x: originX + CGFloat(x) * cell, y: originY + CGFloat(y) * cell, width: cell, height: cell)
}

// Drop shadow and a dark outline one pixel wide.
ctx.saveGState()
ctx.setShadow(offset: CGSize(width: 10, height: 14), blur: 18, color: CGColor(gray: 0, alpha: 0.6))
ctx.setFillColor(rgb(0x0c0d11))
for y in -1...32 { for x in -1...32 where !filled(x, y) {
   if filled(x - 1, y) || filled(x + 1, y) || filled(x, y - 1) || filled(x, y + 1) { ctx.fill(square(x, y)) } } }
ctx.restoreGState()

for y in 0..<mask.count { for x in 0..<mask[y].count where filled(x, y) {
   // The blade runs along x+y = const; the crossguard along x-y = -15.
   let across = x - y + 15
   let side = (x + y) - 32 // <0 is the upper-left face
   let color: UInt32
   if abs(across) <= 2 && y >= 17 {
      color = side < -1 ? 0xf2cf6b : (side > 1 ? 0x8f6519 : 0xd4a33a) // gold guard
   } else if across > 2 {
      color = side < 0 ? 0xf4f8fc : (side > 0 ? 0x7c8795 : 0xbcc6d2) // steel blade
   } else if y >= 28 || x <= 2 {
      color = side < 0 ? 0xf2cf6b : 0xa87a22 // gold pommel
   } else {
      color = side < 0 ? 0x8a5a33 : 0x5a3820 // leather grip
   }
   ctx.setFillColor(rgb(color))
   ctx.fill(square(x, y))
} }

try! rep.representation(using: .png, properties: [:])!.write(to: URL(fileURLWithPath: args[2]))
