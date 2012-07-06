#import "OONetworkBrowserViewManager.h"
#import "OONetworkBrowserBackgroundLayer.h"
#import "OONetworkModel.h"
#import "OOUserBrowserView.h"

@implementation OONetworkBrowserViewManager

- (id)init {
    self = [super init];
    if (self) {
        // Create two arrays : The first is for the data source representation.
        // The second one contains temporary imported images  for thread safeness.
        networks = [[NSMutableArray alloc] init];
        importedNetworks = [[NSMutableArray alloc] init];
    }
    return self;
}

- (void)defineStyle {
    // Allow reordering, animations and set the dragging destination delegate.
    [networkBrowser setAllowsReordering:YES];
    [networkBrowser setAnimates:YES];
    [networkBrowser setAllowsDroppingOnItems:YES];
    [networkBrowser setDraggingDestinationDelegate:self];
    //[networkBrowser setDraggingDestinationDelegate:self[networkBrowser registerForDraggedTypes:[NSArray arrayWithObjects: NSColorPboardType, NSFilenamesPboardType, nil]];];
	
	// customize the appearance
    [networkBrowser setAllowsMultipleSelection:NO];
	[networkBrowser setCellsStyleMask:IKCellsStyleTitled | IKCellsStyleOutlined];
	
	// background layer
	OONetworkBrowserBackgroundLayer *backgroundLayer = [[OONetworkBrowserBackgroundLayer alloc] init];
	[networkBrowser setBackgroundLayer:backgroundLayer];
	backgroundLayer.owner = networkBrowser;
	
	//-- change default font 
	// create a centered paragraph style
	NSMutableParagraphStyle *paraphStyle = [[NSMutableParagraphStyle alloc] init];
	[paraphStyle setLineBreakMode:NSLineBreakByTruncatingTail];
	[paraphStyle setAlignment:NSCenterTextAlignment];
	
	NSMutableDictionary *attributes = [[NSMutableDictionary alloc] init];	
	[attributes setObject:[NSFont systemFontOfSize:9] forKey:NSFontAttributeName]; 
	[attributes setObject:paraphStyle forKey:NSParagraphStyleAttributeName];	
	[attributes setObject:[NSColor blackColor] forKey:NSForegroundColorAttributeName];
	[networkBrowser setValue:attributes forKey:IKImageBrowserCellsTitleAttributesKey];
	
	attributes = [[NSMutableDictionary alloc] init];	
	[attributes setObject:[NSFont boldSystemFontOfSize:9] forKey:NSFontAttributeName]; 
	[attributes setObject:paraphStyle forKey:NSParagraphStyleAttributeName];	
	[attributes setObject:[NSColor whiteColor] forKey:NSForegroundColorAttributeName];
	
	[networkBrowser setValue:attributes forKey:IKImageBrowserCellsHighlightedTitleAttributesKey];	
	
	//change intercell spacing
	[networkBrowser setIntercellSpacing:NSMakeSize(20, 80)];
	
	//change selection color
	[networkBrowser setValue:[NSColor colorWithCalibratedRed:1 green:0 blue:0.5 alpha:1.0] forKey:IKImageBrowserSelectionColorKey];
	
	//set initial zoom value
	[networkBrowser setZoomValue:0.3];
    
}

- (void)updateDatasource
{
    // Update the datasource, add recently imported items.
    [networks addObjectsFromArray:importedNetworks];
	
	// Empty the temporary array.
    [importedNetworks removeAllObjects];
    
    // Reload the image browser, which triggers setNeedsDisplay.
    [networkBrowser reloadData];
}


#pragma mark -
#pragma mark import images from file system

- (BOOL)containsUserId:(NSString*)arg1
{
    for (OONetworkModel* str in importedNetworks) {
        if ([str.uid isEqualToString:arg1])
            return YES;
    }
    return NO; 
}

- (void)addUserWithId:(NSString*)userId
{   
	BOOL addObject = NO;
    
	if (![self containsUserId:userId]) {
		addObject = YES;
	}
	
	if (addObject) {
		// Add a path to the temporary images array.
		OONetworkModel* p = [[OONetworkModel alloc] init];
		p.name = @"Charles Guillot";
        p.image = [NSImage imageNamed:NSImageNameNetwork];
        p.uid = userId;
		[importedNetworks addObject:p];
	}
}

- (void)addUsersWithIds:(NSArray*)userIds
{   
    NSInteger i, n;
	n = [userIds count];
    for (i = 0; i < n; i++)
	{
        NSString* userId = [userIds objectAtIndex:i];
		[self addUserWithId:userId];
    }
    
	// Update the data source in the main thread.
    [self performSelectorOnMainThread:@selector(updateDatasource) withObject:nil waitUntilDone:YES];
}


#pragma mark -
#pragma mark IKImageBrowserDataSource

- (NSUInteger)numberOfItemsInImageBrowser:(IKImageBrowserView*)view
{
	// The item count to display is the datadsource item count.
    return [networks count];
}

- (id)imageBrowser:(IKImageBrowserView *) view itemAtIndex:(NSUInteger) index
{
    return [networks objectAtIndex:index];
}

- (void)imageBrowser:(IKImageBrowserView*)view removeItemsAtIndexes: (NSIndexSet*)indexes
{
	[networks removeObjectsAtIndexes:indexes];
}

- (BOOL)imageBrowser:(IKImageBrowserView*)view moveItemsAtIndexes: (NSIndexSet*)indexes toIndex:(unsigned int)destinationIndex
{
	NSInteger		index;
	NSMutableArray*	temporaryArray;
    
	temporaryArray = [[NSMutableArray alloc] init];
    
	// First remove items from the data source and keep them in a temporary array.
	for (index = [indexes lastIndex]; index != NSNotFound; index = [indexes indexLessThanIndex:index])
	{
		if (index < destinationIndex)
            destinationIndex --;
        
		id obj = [networks objectAtIndex:index];
		[temporaryArray addObject:obj];
		[networks removeObjectAtIndex:index];
	}
    
	// Then insert the removed items at the appropriate location.
	NSInteger n = [temporaryArray count];
	for (index = 0; index < n; index++)
	{
		[networks insertObject:[temporaryArray objectAtIndex:index] atIndex:destinationIndex];
	}
    
	return YES;
}

- (NSUInteger) imageBrowser:(IKImageBrowserView *) aBrowser writeItemsAtIndexes:(NSIndexSet *) itemIndexes toPasteboard:(NSPasteboard *)pasteboard {
    return [itemIndexes count];
}

#pragma mark -
#pragma mark drag n drop 

// -------------------------------------------------------------------------
//	draggingEntered:sender
// ------------------------------------------------------------------------- 
- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender
{
    return NSDragOperationNone;
}
 
// -------------------------------------------------------------------------
//	draggingUpdated:sender
// ------------------------------------------------------------------------- 
- (NSDragOperation)draggingUpdated:(id <NSDraggingInfo>)sender
{
    NSPasteboard *pboard;
    id dsource;
    
    dsource = [sender draggingSource];
    pboard = [sender draggingPasteboard];
    
    if (networkBrowser.dropOperation != IKImageBrowserDropOn)
        return NSDragOperationNone;
    else if ([dsource isKindOfClass:[OOUserBrowserView class]])
        return NSDragOperationCopy;
    else if ([[pboard types] containsObject:NSURLPboardType])
        return NSDragOperationCopy;
    else
        return NSDragOperationNone;
}

// -------------------------------------------------------------------------
//	performDragOperation:sender
// ------------------------------------------------------------------------- 
- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender
{
    NSUInteger networkIndex;
    networkIndex = networkBrowser.indexAtLocationOfDroppedItem;
    OONetworkModel* networkModel = [[networkBrowser cellForItemAtIndex:networkIndex] representedItem];
    
    if ([[sender draggingSource] isKindOfClass:[OOUserBrowserView class]]) {
        
    }
    
    return YES;
}
@end
