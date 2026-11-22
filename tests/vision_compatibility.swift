import Foundation
import simd

@main struct Compatibility {
    static func main() throws {
        let args=CommandLine.arguments
        if args[1]=="encode" {
            var views=ScanViewpoints()
            views.views=[SavedViewpoint(id:UUID(uuidString:"8A2E7674-65E7-4F09-8BD1-C94DB16FD53D")!,name:"Vision entrance",pose:ViewpointPose(position:SIMD3(12.5,-1.75,3),forward:SIMD3(0,0,1)))]
            views.defaultID=views.views[0].id
            try JSONEncoder().encode([args[2]:views]).write(to:URL(fileURLWithPath:args[3]))
        } else {
            let dictionary=try JSONDecoder().decode([String:ScanViewpoints].self,from:Data(contentsOf:URL(fileURLWithPath:args[2])))
            precondition(dictionary.count==1)
            let views=dictionary.values.first!
            precondition(views.isValid && views.views.count==2 && views.startingPose != nil)
            precondition(views.views[1].name=="Quest entrance")
            let settings=try JSONDecoder().decode(ScanSettings.self,from:Data(contentsOf:URL(fileURLWithPath:args[3])))
            precondition(settings.upAxis == .yDown && settings.bubbleRadius==60 && settings.splatFilter == .source)
            // Quest source-coordinate fixture restored through Vision's translated source transform.
            for axis in ScanUpAxis.allCases {
                for scale:Float in [1,0.01,0.001] {
                    var world=sourceOrientation(axis)
                    for i in 0..<3 {world[i] *= scale}
                    world.columns.3=SIMD4(5,2,-7,1)
                    var navigation=NavigationPose()
                    let head=translation(SIMD3(-3,1.7,4))*rotationY(0.7)
                    precondition(navigation.restore(views.views[1].pose,head:head,worldFromSource:world))
                    let expected=world*SIMD4(views.views[1].pose.position,1)
                    let eye=(navigation.worldFromTracking*head).columns.3
                    precondition(simd_length(eye-expected)<0.001)
                    let recaptured=ViewpointPose.capture(head:head,worldFromTracking:navigation.worldFromTracking,worldFromSource:world)!
                    precondition(simd_length(recaptured.position-views.views[1].pose.position)<0.002)
                }
            }
            print("PASS: actual Vision Pro ScanViewpoints and ScanSettings decode Quest JSON; source poses restore with all up axes/scales and translated source transforms")
        }
    }
}
