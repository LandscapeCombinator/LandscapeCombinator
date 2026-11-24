import sys, unreal

n = 0
try:
    unreal.EditorLoadingAndSavingUtils.load_map(sys.argv[1])
    # temporary, in memory only: use a local Overpass server instead of the one saved in the assets
    url = "http://localhost:12345/api/interpreter"
    for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
        try:
            a.get_editor_property("overpass_server")
            a.set_editor_property("overpass_server", url)
        except Exception:
            pass
    tags = sys.argv[2].split(",")
    labels = [l for l in sys.argv[3].split("+") if l] if len(sys.argv) > 3 else []
    all_actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    if labels:
        by_label = {a.get_actor_label(): a for a in all_actors}
        missing = [l for l in labels if l not in by_label]
        if missing: unreal.log_error(f"Actors not found: {missing}")
        sequence = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(unreal.LandscapeCombination, unreal.Vector(0, 0, 0))
        sequence.set_actor_label("LCTest_Sequence")
        sequence.set_editor_property("generators", [unreal.GeneratorWrapper(generator=by_label[l]) for l in labels if l in by_label])
        actors = [sequence]
    else:
        actors = [a for a in all_actors if any(str(t) in tags for t in a.tags)]
    for a in actors:
        print(f"LCTEST_TRIGGER {a.get_actor_label()}", flush=True)
        a.generate_actors_no_prompt()
        n += 1
    print(f"LCTEST_DONE {n}")
except Exception as e:
    unreal.log_error(f"Exception: {e!r}")
