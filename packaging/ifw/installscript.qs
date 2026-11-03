function Component()
{
    component.createOperations();
    if (installer.value("os") === "win") {
        component.addOperation(
            "CreateShortcut",
            "@TargetDir@/bin/player.exe",
            "@StartMenuDir@/Linkamp.lnk");
        component.addOperation(
            "CreateShortcut",
            "@TargetDir@/linkamp-maintenance.exe",
            "@StartMenuDir@/Linkamp Maintenance.lnk");
    }
}
