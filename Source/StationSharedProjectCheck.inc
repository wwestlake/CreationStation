// Debug-only mounted-app acceptance harness. Invokes the same import, project,
// placement and save paths as Station's UI; never opens or changes other projects
// except to restore the project that this process opened on startup.
class MainComponent::SharedProjectCheck final : private juce::Timer
{
public:
    SharedProjectCheck(MainComponent& component, const juce::File& input) : owner(component)
    {
        request = juce::JSON::parse(input);
        output = juce::File(request["output"].toString());
        original = owner.projectSession.getProjectId();
        startTimer(100);
    }
private:
    void require(bool condition,const juce::String& error)
    { if(!condition)throw std::runtime_error(error.toStdString()); }
    void finish(const juce::String& error = {})
    {
        stopTimer();
        auto* result = new juce::DynamicObject();
        result->setProperty("ok",error.isEmpty());result->setProperty("error",error);
        result->setProperty("projectId",owner.projectSession.getProjectId());
        result->setProperty("catalog",creation::assets::toVar(owner.projectSession.getManifest().assetCatalog));
        result->setProperty("timeline",owner.timelineModel.createState().toXmlString());
        result->setProperty("stationState",owner.createProjectStateForSave().toXmlString());
        result->setProperty("clipCount",(int)owner.timelineModel.getClips().size());
        result->setProperty("dirty",owner.projectDirty);
        output.replaceWithText(juce::JSON::toString(juce::var(result),true));
        owner.projectDirty=false;
        if(original.isNotEmpty() && original!=owner.projectSession.getProjectId())owner.openProject(original);
        juce::JUCEApplication::getInstance()->quit();
    }
    void timerCallback() override
    {
        try
        {
            require(++ticks<1200,"Station shared-project check timed out");
            if(stage==0)
            {
                juce::String error;
                if(request["mode"].toString()=="create")
                {
                    require(creation::assets::ProjectWorkspaceService::createProject(owner.suiteSettings,
                        creation::assets::SuiteAppDomain::station,request["name"].toString(),"1.0.0","1.0.0",owner.projectSession,error),error);
                    owner.loadSessionFromDisk(owner.emptyProjectState.createCopy());
                    owner.runLibraryImport({request["audio"].toString(),request["video"].toString()});
                    stage=1;
                }
                else
                {
                    owner.openProject(request["projectId"].toString());
                    require(owner.projectSession.getProjectId()==request["projectId"].toString(),"Station did not open requested shared project");
                    if((bool)request["expectEmpty"])require(owner.timelineModel.getClips().empty(),"Foreign project retained a previous Station arrangement");
                    stage=2;
                }
                return;
            }
            if(stage==1)
            {
                if(owner.progressTask!=nullptr)return;
                owner.refreshProjectAssets();
                const auto assets=owner.projectSession.getManifest().assetCatalog.assets;
                bool audio=false,video=false;
                for(const auto& asset:assets)
                {
                    if(!audio && asset.kind==creation::assets::AssetKind::audio)
                    {owner.placeProjectAssetOnTracker(asset,0);audio=true;}
                    else if(!video && asset.kind==creation::assets::AssetKind::video)
                    {owner.placeProjectAssetOnTracker(asset,0);video=true;}
                }
                require(audio && video,"Actual Station import did not register audio and video");
                require(owner.timelineModel.getClips().size()>=2,"Station did not place shared asset references");
                stage=2;
            }
            if(stage==2)
            {
                owner.saveSessionToDisk(true);
                require(!owner.projectDirty,"Station shared-project save failed");
                for(const auto& asset:owner.projectSession.getManifest().assetCatalog.assets)
                    if(asset.kind==creation::assets::AssetKind::audio || asset.kind==creation::assets::AssetKind::video)
                    {
                        auto stream=owner.projectSession.openEntryStream(asset.logicalPath);char header[16];
                        require(stream!=nullptr && stream->read(header,sizeof(header))>0,"Station could not resolve shared media bytes");
                    }
                finish();
            }
        }
        catch(const std::exception& e){finish(e.what());}
    }
    MainComponent& owner;
    juce::var request;
    juce::File output;
    juce::String original;
    int stage=0,ticks=0;
};
